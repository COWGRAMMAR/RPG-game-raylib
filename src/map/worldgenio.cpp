/**
 * @file worldgenio.cpp
 * @brief Implementasi dari World Generation Save/Load Module
 *
 * File ini berisi implementasi I/O untuk persistensi world generation:
 * save/load runtime state per stage, meta data, dan manajemen slot.
 */

#include "worldgenio.h"
#include "seedmanager.h"
#include "map.h"
#include "propsbehavior.h"
#include "entities.h"
#include "item.h"
#include "screen.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

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

    static std::string GetRuntimePath(int slot)
    {
        return GetSlotDir(slot) + "/runtime.json";
    }

    /** @brief Dapatkan path file map untuk stage tertentu */
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
        const std::string dirs[] = {"saves/enemies", "saves/items"};
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

        nlohmann::json empty;
        std::ofstream rt(GetRuntimePath(saveSlot));
        if (rt.is_open())
            rt << empty.dump(4);

        return true;
    }

    /*=== Runtime State ===*/

    /** @brief Simpan state runtime stage tertentu */
    bool SaveRuntimeState(int stageIndex)
    {
        int slot = g_SeedManager.GetCurrentSlot();
        std::string rtPath = GetRuntimePath(slot);

        nlohmann::json root;
        if (fs::exists(rtPath))
        {
            std::ifstream in(rtPath);
            if (in.is_open())
            {
                try
                {
                    in >> root;
                }
                catch (...)
                {
                    root = nlohmann::json::object();
                }
            }
        }

        std::string key = "stage_" + std::to_string(stageIndex);
        nlohmann::json &stageData = root[key];

        stageData["chests"] = nlohmann::json::array();
        for (auto &pos : chestManager.GetConsumedPositions())
            stageData["chests"].push_back(pos);

        stageData["crates"] = nlohmann::json::array();
        for (auto &pos : crateManager.GetConsumedPositions())
            stageData["crates"].push_back(pos);

        stageData["bombs"] = nlohmann::json::array();
        for (auto &pos : bombManager.GetConsumedPositions())
            stageData["bombs"].push_back(pos);

        stageData["deadEnemies"] = nlohmann::json::array();
        for (auto &entry : Entities::GetDeadEntities())
            stageData["deadEnemies"].push_back(entry);

        stageData["itemDrops"] = nlohmann::json::array();
        for (auto &item : itemData.activeItems)
        {
            nlohmann::json itemJson;
            itemJson["defId"] = item.definitionId;
            itemJson["amount"] = item.amount;
            itemJson["x"] = item.position.x;
            itemJson["y"] = item.position.y;
            stageData["itemDrops"].push_back(itemJson);
        }

        // Barrier state
        stageData["barrier"]["cleared"] = barrierManager.IsCleared();
        stageData["barrier"]["hasReLocked"] = barrierManager.HasReLocked();

        std::ofstream out(rtPath);
        if (!out.is_open())
            return false;
        out << root.dump(4);
        return true;
    }

    /** @brief Muat state runtime stage tertentu */
    bool LoadRuntimeState(int stageIndex)
    {
        int slot = g_SeedManager.GetCurrentSlot();
        std::string rtPath = GetRuntimePath(slot);

        if (!fs::exists(rtPath))
            return false;

        std::ifstream in(rtPath);
        if (!in.is_open())
            return false;

        nlohmann::json root;
        try
        {
            in >> root;
        }
        catch (...)
        {
            return false;
        }

        std::string key = "stage_" + std::to_string(stageIndex);
        if (!root.contains(key))
            return false;

        nlohmann::json &stageData = root[key];

        if (stageData.contains("chests"))
        {
            std::unordered_set<std::string> positions;
            for (auto &pos : stageData["chests"])
                positions.insert(pos.get<std::string>());
            chestManager.SetConsumedPositions(positions);
        }

        if (stageData.contains("crates"))
        {
            std::unordered_set<std::string> positions;
            for (auto &pos : stageData["crates"])
                positions.insert(pos.get<std::string>());
            crateManager.SetConsumedPositions(positions);
        }

        if (stageData.contains("bombs"))
        {
            std::unordered_set<std::string> positions;
            for (auto &pos : stageData["bombs"])
                positions.insert(pos.get<std::string>());
            bombManager.SetConsumedPositions(positions);
        }

        if (stageData.contains("deadEnemies"))
        {
            std::set<std::string> entries;
            for (auto &entry : stageData["deadEnemies"])
                entries.insert(entry.get<std::string>());
            Entities::SetDeadEntities(entries);
        }

        if (stageData.contains("itemDrops"))
        {
            itemData.activeItems.clear();
            for (auto &itemJson : stageData["itemDrops"])
            {
                ItemSpawn item;
                item.definitionId = itemJson["defId"];
                item.amount = itemJson["amount"];
                item.position.x = itemJson["x"];
                item.position.y = itemJson["y"];
                itemData.activeItems.push_back(item);
            }
        }

        // Barrier state
        if (stageData.contains("barrier"))
        {
            auto &b = stageData["barrier"];
            if (b.contains("cleared"))
                barrierManager.SetCleared(b["cleared"].get<bool>());
            if (b.contains("hasReLocked"))
                barrierManager.SetHasReLocked(b["hasReLocked"].get<bool>());
        }

        return true;
    }

    /*=== Transitions ===*/

    /** @brief Pindah ke stage berikutnya */
    void NextStage()
    {
        int oldStage = g_SeedManager.GetCurrentStage();
        SaveRuntimeState(oldStage);

        TraceLog(LOG_INFO, "NextStage: %d -> %d", oldStage + 1, oldStage + 2);

        if (oldStage >= SeedManager::SEED_COUNT - 1)
        {
            // Sudah stage terakhir (boss) — balik lobby, reset run
            g_SeedManager.ResetRun();
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
        SaveRuntimeState(oldStage);

        int targetStage = g_SeedManager.GoBackStage();
        g_SeedManager.SetCurrentStage(targetStage);
        TraceLog(LOG_INFO, "PrevStage: %d -> %d", oldStage + 1, targetStage + 1);

        g_SeedManager.SaveMeta(GetMetaPath(g_SeedManager.GetCurrentSlot()));

        std::string stagePath = GetStagePath(targetStage);
        SwitchMap(stagePath.c_str(), "finish");

        TrimStageStack();
    }

} // namespace WorldgenIO
