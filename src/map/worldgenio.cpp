/**
 * @file worldgenio.cpp
 * @brief Implementasi dari World Generation Save/Load Module
 *
 * File ini berisi implementasi I/O untuk persistensi world generation:
 * save/load runtime state per stage, meta data, dan manajemen slot.
 */

#include "worldgenio.h"
#include "seedmanager.h"
#include "game_state_saver.h"
#include "savemanager.h"
#include "map.h"
#include "propsbehavior.h"
#include "entities.h"
#include "item.h"
#include "screen.h"
#include "ui/mainMenu.h"
#include "systems/combatTurn.h"
#include "systems/audioManager.h"
#include "map/minimap.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

static const char *WORLDSEED_DIR = "assets/maps/World_generation/worldseed";
static const char *BG_MAP = "assets/maps/World_generation/background_map.json";

namespace WorldgenIO
{
    /*=== Helpers Path per Slot ===*/

    static std::string GetSlotDir(int slot)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s/save_%d", WORLDSEED_DIR, slot);
        return std::string(buf);
    }

    /** @brief Dapatkan path meta.json untuk slot tertentu */
    std::string GetMetaPath(int slot)
    {
        return GetSlotDir(slot) + "/meta.json";
    }

    /**
     * @brief Dapatkan path file map untuk stage tertentu
     * @brief Dapatkan path file map untuk stage tertentu
     * @param stageIndex Index stage (0-based)
     * @return Path lengkap ke file JSON map di slot aktif
     */
    std::string GetStagePath(int stageIndex)
    {
        int slot = g_SeedManager.GetCurrentSlot();
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/maps/stage_%d.json", GetSlotDir(slot).c_str(), stageIndex + 1);
        return std::string(buf);
    }

    /*=== Slot Management ===*/

    /** @brief Cari slot kosong berikutnya (scan folder save_*) */
    int GetNextAvailableSlot()
    {
        int maxSlot = 0;
        std::string base = WORLDSEED_DIR;
        if (fs::exists(base))
        {
            for (auto &entry : fs::directory_iterator(base))
            {
                if (!entry.is_directory())
                    continue;
                std::string name = entry.path().filename().string();
                if (name.rfind("save_", 0) == 0)
                {
                    int slot = std::stoi(name.substr(5));
                    if (slot > maxSlot)
                        maxSlot = slot;
                }
            }
        }
        return maxSlot + 1;
    }

    /** @brief Dapatkan nomor slot tertinggi yang tersedia */
    int GetTopSlot()
    {
        int maxSlot = 0;
        std::string base = WORLDSEED_DIR;
        if (fs::exists(base))
        {
            for (auto &entry : fs::directory_iterator(base))
            {
                if (!entry.is_directory())
                    continue;
                std::string name = entry.path().filename().string();
                if (name.rfind("save_", 0) == 0)
                {
                    int slot = std::stoi(name.substr(5));
                    if (slot > maxSlot)
                        maxSlot = slot;
                }
            }
        }
        return maxSlot;
    }

    /*=== Init ===*/

    /** @brief Inisialisasi run baru di slot tertentu */
    /*=== Cache Management ===*/

    /** @brief Hapus hanya file .cache dari folder saves/enemies dan saves/items */
    void ClearCache()
    {
        const std::string dirs[] = {"saves/cache/enemies", "saves/cache/items"};
        for (const auto &dir : dirs)
        {
            if (!fs::exists(dir))
                continue;
            for (const auto &entry : fs::directory_iterator(dir))
            {
                if (entry.path().extension() == ".cache")
                {
                    fs::remove(entry.path());
                }
            }
        }
    }

    bool InitRun(int saveSlot)
    {
        ClearCache();

        g_SeedManager.InitRun(saveSlot);

        std::string slotDir = GetSlotDir(saveSlot);
        std::string mapsDir = slotDir + "/maps";
        fs::create_directories(mapsDir);

        for (int i = 0; i < SeedManager::SEED_COUNT; i++)
        {
            char dst[256];
            snprintf(dst, sizeof(dst), "%s/stage_%d.json", mapsDir.c_str(), i + 1);
            fs::copy_file(BG_MAP, dst, fs::copy_options::overwrite_existing);

            // Fix relative texture paths — file jadi 3 folder lebih dalam dari asli
            std::ifstream in(dst);
            if (in.is_open())
            {
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                // Cari "image":"../../textures/" dan ganti jadi "../../../../textures/"
                size_t pos = 0;
                const std::string oldPrefix = "\"../../textures/";
                const std::string newPrefix = "\"../../../../textures/";
                while ((pos = content.find(oldPrefix, pos)) != std::string::npos)
                {
                    content.replace(pos, oldPrefix.length(), newPrefix);
                    pos += newPrefix.length();
                }

                std::ofstream out(dst);
                if (out.is_open())
                    out << content;
            }
        }

        g_SeedManager.SaveMeta(GetMetaPath(saveSlot));

        // Runtime state files sekarang dikelola SaveManager via saves/slot_N/checkpoints/
        return true;
    }

    /*=== Transitions ===*/

    /** @brief Pindah ke stage berikutnya */
    void NextStage()
    {
        int oldStage = g_SeedManager.GetCurrentStage();

        TraceLog(LOG_INFO, "NextStage: %d -> %d", oldStage + 1, oldStage + 2);

        if (oldStage >= SeedManager::SEED_COUNT - 1)
        {
            // Sudah stage terakhir (boss) — balik lobby, reset run
            TurnCombat::Shutdown();
            Entities::Clear();
            Entities::ClearDeadEntities();
            UnloadMap();
            mapHistoryStack.Clear();
            MinimapSystem::Shutdown();
            g_Minimap.fogCache.clear();

            CloseTextures();
            AudioManager::UnloadMusic();
            gState->assetsLoaded = false;
            InputInstance.ResetMenuFlags();
            g_SeedManager.ResetRun();
            InitMainMenu(gState);
            gState->currentScreen = MAIN_MENU;
            return;
        }

        g_SeedManager.NextStage();
        g_SeedManager.SaveMeta(GetMetaPath(g_SeedManager.GetCurrentSlot()));

        std::string stagePath = GetStagePath(g_SeedManager.GetCurrentStage());
        SwitchMap(stagePath.c_str(), "start");

        TrimStageStack();
    }

    /** @brief Kembali ke stage sebelumnya */
    void PrevStage()
    {
        if (!g_SeedManager.CanGoBack())
            return;

        int oldStage = g_SeedManager.GetCurrentStage();

        int targetStage = g_SeedManager.GoBackStage();
        g_SeedManager.SetCurrentStage(targetStage);
        TraceLog(LOG_INFO, "PrevStage: %d -> %d", oldStage + 1, targetStage + 1);

        g_SeedManager.SaveMeta(GetMetaPath(g_SeedManager.GetCurrentSlot()));

        std::string stagePath = GetStagePath(targetStage);
        SwitchMap(stagePath.c_str(), "finish");

        TrimStageStack();
    }

    /** @brief Helper baca worldgenSlot dari file JSON manapun */
    static int ReadWorldgenSlotFromFile(const std::string &filePath)
    {
        if (!fs::exists(filePath))
            return -1;
        try
        {
            std::ifstream in(filePath);
            nlohmann::json root;
            in >> root;
            return root.value("worldgenSlot", -1);
        }
        catch (...)
        {
            return -1;
        }
    }

    void CleanupOrphanedSlots()
    {
        const int TOTAL_SAVE_SLOTS = 6; // Hanya manual saves (slot 0-5)
        std::unordered_set<int> activeSlots;

        // Current active run
        if (g_SeedManager.IsRunActive())
            activeSlots.insert(g_SeedManager.GetCurrentSlot());

        // Scan save files; new format (manual/snapshot.json)
        for (int i = 0; i < TOTAL_SAVE_SLOTS; i++)
        {
            int ws = ReadWorldgenSlotFromFile(SaveManager::GetManualPath(i));
            if (ws >= 0)
                activeSlots.insert(ws);
        }

        // Delete orphaned worldseed save directories
        if (!fs::exists(WORLDSEED_DIR))
            return;

        int cleanedCount = 0;
        for (auto &entry : fs::directory_iterator(WORLDSEED_DIR))
        {
            if (!entry.is_directory())
                continue;

            std::string dirName = entry.path().filename().string();
            if (dirName.rfind("save_", 0) != 0)
                continue;

            int slotNum = std::stoi(dirName.substr(5));
            if (activeSlots.find(slotNum) == activeSlots.end())
            {
                fs::remove_all(entry.path());
                cleanedCount++;
            }
        }

        if (cleanedCount > 0)
            TraceLog(LOG_INFO, "Cleaned up %d orphan worldgen slot(s)", cleanedCount);
    }
} // namespace WorldgenIO
