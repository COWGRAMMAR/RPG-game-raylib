/**
 * @file game_state_saver.cpp
 * @brief Implementasi Modul Preservasi State Game
 *
 * Handle save dan restore seluruh state game world.
 */

#include "game_state_saver.h"
#include "seedmanager.h"
#include "map.h"
#include "propsbehavior.h"
#include "entities.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <unordered_set>

using json = nlohmann::json;

/*==============================================================================
 * Global Saved State Variables
 *==============================================================================*/

/**
 * @brief State player yang tersimpan
 */
SavedPlayerState savedPlayerState;

/**
 * @brief Daftar state enemy yang tersimpan
 */
std::vector<SavedEnemyState> savedEnemyStates;

/**
 * @brief Daftar state item yang tersimpan
 */
std::vector<SavedItemState> savedItemStates;

/**
 * @brief State map yang tersimpan
 */
SavedMapState savedMapState;

/**
 * @brief Flag menandakan apakah ada state tersimpan
 */
bool hasSavedState = false;
static bool worldgenPending = false;

/*==============================================================================
 * Active Slot Tracking
 *==============================================================================*/

/**
 * @brief Slot save yang sedang aktif
 * -1 = tidak ada slot aktif (default/main menu)
 * 0-4 = slot manual yang aktif
 */
int g_ActiveSaveSlot = -1;

/**
 * @brief Flag apakah ada slot yang sedang aktif
 */
bool g_SaveSlotActive = false;

/**
 * @brief Set slot save yang aktif.
 * @param slot Nomor slot (0-4), -1 untuk menonaktifkan
 */
void SetActiveSlot(int slot)
{
    g_ActiveSaveSlot = slot;
    g_SaveSlotActive = (slot >= 0 && slot <= 4);
    TraceLog(LOG_INFO, "[SetActiveSlot] Slot %d %s", slot, g_SaveSlotActive ? "activated" : "deactivated");
}

/**
 * @brief Dapatkan slot save yang aktif.
 * @return Nomor slot aktif, -1 jika tidak ada
 */
int GetActiveSlot(void)
{
    return g_ActiveSaveSlot;
}

/**
 * @brief Cek apakah ada slot yang sedang aktif.
 * @return true jika ada slot aktif
 */
bool IsSlotActive(void)
{
    return g_SaveSlotActive;
}

/**
 * @brief Dapatkan path file save untuk slot dan tipe tertentu.
 * @param slot Nomor slot (0-4)
 * @param type Tipe save ("manual" atau "autosave")
 * @return Path lengkap file save
 * @note Slot 2, type "manual" -> "saves/slot_2/manual/manual.json"
 *       Slot 2, type "autosave" -> "saves/slot_2/autosave/"
 */
std::string GetSlotPath(int slot, const std::string& type)
{
    char buf[128];
    if (type == "manual")
    {
        snprintf(buf, sizeof(buf), "saves/slot_%d/manual/manual.json", slot);
    }
    else if (type == "autosave")
    {
        snprintf(buf, sizeof(buf), "saves/slot_%d/autosave", slot);
    }
    else
    {
        snprintf(buf, sizeof(buf), "saves/slot_%d/%s", slot, type.c_str());
    }
    return std::string(buf);
}

/*==============================================================================
 * Slot Directory Utilities
 *==============================================================================*/

/**
 * @brief Pastikan direktori slot save tersedia.
 * @param slot Nomor slot (0-4)
 * @details Membuat struktur direktori:
 *          - saves/slot_N/manual/
 *          - saves/slot_N/autosave/
 *          - saves/slot_N/enemies/
 *          - saves/slot_N/items/
 *          Tidak melakukan apa-apa jika direktori sudah ada.
 */
void EnsureSlotDirectory(int slot)
{
    const char* subdirs[] = {"manual", "autosave", "enemies", "items"};
    for (const char* subdir : subdirs)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "saves/slot_%d/%s", slot, subdir);
        std::filesystem::create_directories(buf);
        TraceLog(LOG_INFO, "[EnsureSlotDirectory] %s", buf);
    }
}

/*==============================================================================
 * File I/O Functions
 *==============================================================================*/

/**
 * WriteSaveFile - Serialize all global saved state to a JSON file using atomic write.
 */
bool WriteSaveFile(const std::string& path)
{
    TraceLog(LOG_INFO, "Writing save to: %s", path.c_str());
    json root;

    // Version and timestamp metadata
    root["version"] = SAVE_VERSION;
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&t));
    root["timestamp"] = std::string(buf);

    // Multi-slot metadata
    root["slotIndex"] = (g_ActiveSaveSlot >= 0) ? g_ActiveSaveSlot : -1;  ///< -1 = unassigned, 0-4 = manual slot
    root["saveType"] = "manual";       ///< "manual" or "autosave"
    root["playTime"] = 0.0f;           ///< Placeholder: gameplay timer deferred
    root["mapDisplayName"] = savedPlayerState.mapDisplayName;
    root["worldgenSlot"] = (g_ActiveSaveSlot >= 0) ? g_ActiveSaveSlot : -1;  ///< -1 = unassigned, maps to worldseed/save_N/

    // Player section
    json playerJson;
    playerJson["position"] = {savedPlayerState.position.x, savedPlayerState.position.y};
    playerJson["health"] = savedPlayerState.health;
    playerJson["maxHealth"] = savedPlayerState.maxHealth;
    playerJson["mana"] = savedPlayerState.mana;
    playerJson["maxMana"] = savedPlayerState.maxMana;

    json hotbarJson = json::array();
    for (int i = 0; i < 4; i++)
    {
        json item;
        item["definitionId"] = savedPlayerState.hotbar[i].definitionId;
        item["amount"] = savedPlayerState.hotbar[i].amount;
        hotbarJson.push_back(item);
    }
    playerJson["hotbar"] = hotbarJson;

    json bagJson = json::array();
    for (int i = 0; i < 12; i++)
    {
        json item;
        item["definitionId"] = savedPlayerState.bag[i].definitionId;
        item["amount"] = savedPlayerState.bag[i].amount;
        bagJson.push_back(item);
    }
    playerJson["bag"] = bagJson;

    json animJson;
    animJson["state"] = savedPlayerState.animState.state;
    animJson["direction"] = savedPlayerState.animState.direction;
    animJson["isDead"] = savedPlayerState.animState.isDead;
    animJson["activeSlot"] = savedPlayerState.animState.activeSlot;
    playerJson["animState"] = animJson;

    playerJson["showFPS"] = gState->showFPS;

    playerJson["dashCooldown"] = savedPlayerState.dashCooldown;
    playerJson["manaRegenTimer"] = savedPlayerState.manaRegenTimer;
    playerJson["swingAttack"] = savedPlayerState.swingAttack;

    root["player"] = playerJson;

    // Enemies section
    json enemiesJson = json::array();
    for (const auto& enemy : savedEnemyStates)
    {
        json e;
        e["position"] = {enemy.position.x, enemy.position.y};
        e["enemyName"] = enemy.enemyName;
        e["currentHP"] = enemy.currentHP;
        e["isAlive"] = enemy.isAlive;
        e["maxHealth"] = enemy.maxHealth;
        e["aiState"] = enemy.aiState;
        e["patrolTargetX"] = enemy.patrolTargetX;
        e["patrolTargetY"] = enemy.patrolTargetY;
        e["patrolTimer"] = enemy.patrolTimer;
        e["mapObjectID"] = enemy.mapObjectID;
        e["spawnPoint"] = enemy.spawnPoint;
        e["healthRegenTimer"] = enemy.healthRegenTimer;
        e["attackCooldownTimer"] = enemy.attackCooldownTimer;
        e["uuid"] = enemy.uuid;
        enemiesJson.push_back(e);
    }
    root["enemies"] = enemiesJson;

    // Items section
    json itemsJson = json::array();
    for (const auto& item : savedItemStates)
    {
        json it;
        it["position"] = {item.position.x, item.position.y};
        it["isPickedUp"] = item.isPickedUp;
        it["definitionId"] = item.definitionId;
        it["amount"] = item.amount;
        it["uuid"] = item.uuid;
        itemsJson.push_back(it);
    }
    root["items"] = itemsJson;

    // Map section
    json mapJson;
    mapJson["mapPath"] = savedMapState.mapPath;
    mapJson["cameraTarget"] = {savedMapState.cameraTarget.x, savedMapState.cameraTarget.y};
    mapJson["cameraZoom"] = savedMapState.cameraZoom;

    json deadEntitiesJson = json::array();
    for (const auto& name : savedMapState.deadEntities)
        deadEntitiesJson.push_back(name);
    mapJson["deadEntities"] = deadEntitiesJson;

    json chestsOpenedJson = json::array();
    for (const auto& pos : savedMapState.chestsOpened)
        chestsOpenedJson.push_back(pos);
    mapJson["chestsOpened"] = chestsOpenedJson;

    json mapHistoryJson = json::array();
    for (const auto& entry : savedMapState.mapHistory)
    {
        json h;
        h["mapPath"] = entry.mapPath;
        h["doorName"] = entry.doorName;
        mapHistoryJson.push_back(h);
    }
    mapJson["mapHistory"] = mapHistoryJson;

    mapJson["bombConsumedPositions"] = savedMapState.bombConsumedPositions;
    mapJson["crateConsumedPositions"] = savedMapState.crateConsumedPositions;

    root["map"] = mapJson;

    // Atomic write: write to .tmp then rename
    std::string tmpPath = path + ".tmp";
    if (std::filesystem::exists(tmpPath))
        std::filesystem::remove(tmpPath);
    std::ofstream file(tmpPath);
    file << root.dump(4);
    file.close();

    std::filesystem::rename(tmpPath, path);

    bool saveResult = std::filesystem::exists(path);
    TraceLog(LOG_INFO, "Save to %s: %s", path.c_str(), saveResult ? "SUCCESS" : "FAILED");
    return saveResult;
}

/**
 * WriteAutosave - Simpan state game ke direktori autosave per-slot.
 * Membuat nama file dengan timestamp (autosave_DD-MM-YYYY-HH-MM-SS.json)
 * agar autosave tidak saling menimpa. Maksimal 5 file autosave per slot --
 * jika lebih, file terlama akan dihapus.
 * Path tujuan: saves/slot_N/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json
 */
bool WriteAutosave(const std::string&)
{
    if (g_ActiveSaveSlot < 0) return false;

    EnsureSlotDirectory(g_ActiveSaveSlot);

    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "autosave_%d-%m-%Y-%H-%M-%S.json", std::localtime(&t));
    std::string autosaveName(buf);
    std::string dir = GetSlotPath(g_ActiveSaveSlot, "autosave");
    TraceLog(LOG_INFO, "Autosaving to %s/%s", dir.c_str(), autosaveName.c_str());
    SaveGameState(gState);

    bool result = WriteSaveFile(dir + "/" + autosaveName);

    // Batasi maksimal 5 file autosave per slot — hapus file terlama jika melebihi
    constexpr int MAX_AUTOSAVE_SLOTS = 5;
    std::vector<std::filesystem::path> autosaveFiles;
    if (std::filesystem::exists(dir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.path().filename().string().find("autosave_") == 0 && entry.path().extension() == ".json")
                autosaveFiles.push_back(entry.path());
        }
    }
    if (autosaveFiles.size() > MAX_AUTOSAVE_SLOTS)
    {
        std::sort(autosaveFiles.begin(), autosaveFiles.end(),
            [](const auto& a, const auto& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });
        // Hapus file terlama hingga hanya MAX_AUTOSAVE_SLOTS tersisa
        for (size_t i = 0; i < autosaveFiles.size() - MAX_AUTOSAVE_SLOTS; i++)
        {
            std::filesystem::remove(autosaveFiles[i]);
            TraceLog(LOG_INFO, "Pruned old autosave: %s", autosaveFiles[i].filename().string().c_str());
        }
    }

    return result;
}

/**
 * ReadSaveFile - Read JSON save file and deserialize into global saved state.
 */
bool ReadSaveFile(const std::string& path)
{
    TraceLog(LOG_INFO, "Reading save from: %s", path.c_str());
    if (!std::filesystem::exists(path))
    {
        TraceLog(LOG_WARNING, "Save file not found: %s", path.c_str());
        return false;
    }

    try
    {
        std::ifstream file(path);
        json root = json::parse(file);

        // Validate required fields exist
        if (!root.contains("version") || !root.contains("player") || !root.contains("map"))
        {
            TraceLog(LOG_WARNING, "Save file missing required fields: %s", path.c_str());
            return false;
        }

        // Validate version — only v3 is supported
        int version = root.at("version").get<int>();
        if (version == 2)
        {
            TraceLog(LOG_WARNING, "Save file is v2 format (no longer supported): %s. Migration needed.", path.c_str());
            return false;
        }
        if (version != SAVE_VERSION)
        {
            TraceLog(LOG_WARNING, "Save file version mismatch (expected %d, got %d): %s", SAVE_VERSION, version, path.c_str());
            return false;
        }

        // Read player section
        const auto& player = root.at("player");
        savedPlayerState.position.x = player.at("position")[0].get<float>();
        savedPlayerState.position.y = player.at("position")[1].get<float>();
        TraceLog(LOG_INFO, "LOAD: read position = (%.2f, %.2f) from %s", savedPlayerState.position.x, savedPlayerState.position.y, path.c_str());
        savedPlayerState.health = player.value("health", 100.0f);
        savedPlayerState.maxHealth = player.value("maxHealth", 100.0f);
        savedPlayerState.mana = player.value("mana", 100.0f);
        savedPlayerState.maxMana = player.value("maxMana", 100.0f);

        if (player.contains("hotbar"))
        {
            const auto& hotbar = player.at("hotbar");
            for (int i = 0; i < 4 && i < (int)hotbar.size(); i++)
            {
                savedPlayerState.hotbar[i].definitionId = hotbar[i].value("definitionId", -1);
                savedPlayerState.hotbar[i].amount = hotbar[i].value("amount", 0);
            }
        }

        if (player.contains("bag"))
        {
            const auto& bag = player.at("bag");
            for (int i = 0; i < 12 && i < (int)bag.size(); i++)
            {
                savedPlayerState.bag[i].definitionId = bag[i].value("definitionId", -1);
                savedPlayerState.bag[i].amount = bag[i].value("amount", 0);
            }
        }

        if (player.contains("animState"))
        {
            const auto& anim = player.at("animState");
            savedPlayerState.animState.state = anim.value("state", 0);
            savedPlayerState.animState.direction = anim.value("direction", 0);
            savedPlayerState.animState.isDead = anim.value("isDead", false);
            savedPlayerState.animState.activeSlot = anim.value("activeSlot", 0);
        }

        if (player.contains("showFPS"))
            gState->showFPS = player.at("showFPS").get<bool>();

        // Read new player fields
        savedPlayerState.dashCooldown = player.value("dashCooldown", 0.0f);
        savedPlayerState.manaRegenTimer = player.value("manaRegenTimer", 0.0f);
        if (player.contains("swingAttack"))
            savedPlayerState.swingAttack = player.at("swingAttack");

        // Read enemies section
        savedEnemyStates.clear();
        if (root.contains("enemies"))
        {
            for (const auto& e : root.at("enemies"))
            {
                SavedEnemyState enemy;
                enemy.position.x = e.at("position")[0].get<float>();
                enemy.position.y = e.at("position")[1].get<float>();
                enemy.enemyName = e.value("enemyName", "");
                enemy.currentHP = e.value("currentHP", 0);
                enemy.isAlive = e.value("isAlive", true);
                enemy.maxHealth = e.value("maxHealth", 100.0f);
                enemy.aiState = e.value("aiState", 0);
                enemy.patrolTargetX = e.value("patrolTargetX", 0.0f);
                enemy.patrolTargetY = e.value("patrolTargetY", 0.0f);
                enemy.patrolTimer = e.value("patrolTimer", 0.0f);
                enemy.mapObjectID = e.value("mapObjectID", -1);
                enemy.spawnPoint = e.value("spawnPoint", nlohmann::json({{"x", 0}, {"y", 0}}));
                enemy.healthRegenTimer = e.value("healthRegenTimer", 2.0f);
                enemy.attackCooldownTimer = e.value("attackCooldownTimer", 0.0f);
                enemy.uuid = e.value("uuid", "");
                savedEnemyStates.push_back(enemy);
            }
        }

        // Read items section
        savedItemStates.clear();
        if (root.contains("items"))
        {
            for (const auto& it : root.at("items"))
            {
                SavedItemState item;
                item.position.x = it.at("position")[0].get<float>();
                item.position.y = it.at("position")[1].get<float>();
                item.isPickedUp = it.value("isPickedUp", false);
                item.definitionId = it.value("definitionId", -1);
                item.amount = it.value("amount", 1);
                item.uuid = it.value("uuid", "");
                savedItemStates.push_back(item);
            }
        }

        // Read map section
        const auto& map = root.at("map");
        savedMapState.mapPath = map.value("mapPath", "");
        savedMapState.cameraTarget.x = map.at("cameraTarget")[0].get<float>();
        savedMapState.cameraTarget.y = map.at("cameraTarget")[1].get<float>();
        savedMapState.cameraZoom = map.value("cameraZoom", 1.0f);

        savedMapState.deadEntities.clear();
        if (map.contains("deadEntities"))
        {
            for (const auto& d : map.at("deadEntities"))
                savedMapState.deadEntities.push_back(d.get<std::string>());
        }

        savedMapState.chestsOpened.clear();
        if (map.contains("chestsOpened"))
        {
            for (const auto& c : map.at("chestsOpened"))
                savedMapState.chestsOpened.push_back(c.get<std::string>());
        }

        savedMapState.mapHistory.clear();
        if (map.contains("mapHistory"))
        {
            for (const auto& h : map.at("mapHistory"))
            {
                MapSystem::MapHistoryEntry entry;
                entry.mapPath = h.value("mapPath", "");
                entry.doorName = h.value("doorName", "");
                savedMapState.mapHistory.push_back(entry);
            }
        }

        // Read bomb/crate consumed positions
        if (map.contains("bombConsumedPositions"))
            savedMapState.bombConsumedPositions = map.at("bombConsumedPositions");
        if (map.contains("crateConsumedPositions"))
            savedMapState.crateConsumedPositions = map.at("crateConsumedPositions");

        // Read multi-slot metadata (v3+ fields, optional for backward compat)
        if (root.contains("slotIndex"))
            savedPlayerState.slotIndex = root.at("slotIndex").get<int>();
        if (root.contains("saveType"))
            savedPlayerState.saveType = root.value("saveType", "manual");
        if (root.contains("playTime"))
            savedPlayerState.playTime = root.value("playTime", 0.0f);
        if (root.contains("mapDisplayName"))
            savedPlayerState.mapDisplayName = root.value("mapDisplayName", "");
        if (root.contains("worldgenSlot"))
            savedPlayerState.worldgenSlot = root.value("worldgenSlot", -1);

        hasSavedState = true;
        TraceLog(LOG_INFO, "Save file %s loaded successfully (%d enemies, %d items)", path.c_str(), (int)savedEnemyStates.size(), (int)savedItemStates.size());
        return true;
    }
    catch (const json::parse_error&)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - parse error: %s", path.c_str());
        return false;
    }
    catch (const json::out_of_range&)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - missing field: %s", path.c_str());
        return false;
    }
    catch (const json::type_error&)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - type error: %s", path.c_str());
        return false;
    }
}

/**
 * HasSaveFile - Check if save file exists and has content.
 */
bool HasSaveFile(const std::string& path)
{
    TraceLog(LOG_INFO, "Checking save file: %s", path.c_str());
    return std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
}

/**
 * DeleteSaveFile - Remove save file if it exists.
 */
void DeleteSaveFile(const std::string& path)
{
    TraceLog(LOG_INFO, "Deleting save file: %s", path.c_str());
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
}

/*==============================================================================
 * Migration v2 -> v3
 *==============================================================================*/

/**
 * @brief Periksa apakah migrasi v2->v3 diperlukan.
 * @return true jika file saves/manual/slot0.json ada DAN sentinel
 *         saves/.migration_completed_v3 tidak ada.
 */
bool NeedsMigration(void)
{
    bool oldSaveExists = std::filesystem::exists("saves/manual/slot0.json");
    bool sentinelExists = std::filesystem::exists("saves/.migration_completed_v3");
    bool needs = oldSaveExists && !sentinelExists;
    TraceLog(LOG_INFO, "[Migration] NeedsMigration: oldSave=%d sentinel=%d -> %s",
             oldSaveExists, sentinelExists, needs ? "YES" : "NO");
    return needs;
}

/**
 * @brief Tandai migrasi sebagai selesai dengan menulis sentinel.
 * Membuat file kosong saves/.migration_completed_v3.
 */
void MarkMigrationComplete(void)
{
    std::ofstream sentinel("saves/.migration_completed_v3");
    if (sentinel.is_open())
    {
        sentinel.close();
        TraceLog(LOG_INFO, "[Migration] Sentinel written: saves/.migration_completed_v3");
    }
    else
    {
        TraceLog(LOG_WARNING, "[Migration] Failed to write sentinel file!");
    }
}

/**
 * @brief Jalankan pipeline migrasi v2->v3.
 * @return true jika semua langkah berhasil, false jika ada yang gagal.
 *
 * Urutan:
 *  1. Copy saves/manual/slot0.json -> saves/slot_0/manual/manual.json (v2->v3 upgrade)
 *  2. Pindahkan saves/enemies/ -> saves/slot_0/enemies/
 *  3. Pindahkan saves/items/ -> saves/slot_0/items/
 *  4. Hapus direktori lama + tulis sentinel
 *
 * Atomic: jika langkah manapun gagal, abort tanpa cleanup.
 * Sentinel mencegah migrasi ulang pada launch berikutnya.
 */
bool RunMigration(void)
{
    TraceLog(LOG_INFO, "[Migration] Starting v2->v3 migration pipeline...");

    /*----------------------------------------------------------------------
     * Task 14: Copy saves/manual/slot0.json -> saves/slot_0/manual/manual.json
     * dengan upgrade v2->v3.
     *----------------------------------------------------------------------*/
    {
        if (!std::filesystem::exists("saves/manual/slot0.json"))
        {
            TraceLog(LOG_WARNING, "[Migration] slot0.json not found, aborting.");
            return false;
        }

        try
        {
            std::ifstream srcFile("saves/manual/slot0.json");
            json root = json::parse(srcFile);
            srcFile.close();

            // Upgrade version from 2 to 3
            root["version"] = 3;

            // Add v3 fields if not present
            if (!root.contains("slotIndex"))     root["slotIndex"] = 0;
            if (!root.contains("saveType"))      root["saveType"] = "manual";
            if (!root.contains("playTime"))      root["playTime"] = 0.0f;
            if (!root.contains("mapDisplayName")) root["mapDisplayName"] = "";
            if (!root.contains("worldgenSlot"))  root["worldgenSlot"] = -1;

            // Ensure target directory exists
            std::filesystem::create_directories("saves/slot_0/manual");

            // Atomic write: .tmp + rename
            std::string targetPath = "saves/slot_0/manual/manual.json";
            std::string tmpPath = targetPath + ".tmp";
            if (std::filesystem::exists(tmpPath))
                std::filesystem::remove(tmpPath);

            std::ofstream outFile(tmpPath);
            outFile << root.dump(4);
            outFile.close();

            std::filesystem::rename(tmpPath, targetPath);

            if (!std::filesystem::exists(targetPath))
            {
                TraceLog(LOG_WARNING, "[Migration] Failed to write %s", targetPath.c_str());
                return false;
            }

            TraceLog(LOG_INFO, "[Migration] Task 14: Copied slot0.json -> %s (v2->v3)", targetPath.c_str());
        }
        catch (const std::exception& e)
        {
            TraceLog(LOG_WARNING, "[Migration] Task 14 failed: %s", e.what());
            return false;
        }
    }

    /*----------------------------------------------------------------------
     * Task 15: Pindahkan saves/enemies/ -> saves/slot_0/enemies/
     *----------------------------------------------------------------------*/
    {
        if (std::filesystem::exists("saves/enemies") && std::filesystem::is_directory("saves/enemies"))
        {
            EnsureSlotDirectory(0); // ensures saves/slot_0/enemies/ exists

            try
            {
                for (const auto& entry : std::filesystem::directory_iterator("saves/enemies"))
                {
                    if (entry.path().extension() == ".json")
                    {
                        std::string dest = "saves/slot_0/enemies/" + entry.path().filename().string();
                        std::filesystem::rename(entry.path(), dest);
                        TraceLog(LOG_INFO, "[Migration] Moved %s -> %s",
                                 entry.path().filename().string().c_str(), dest.c_str());
                    }
                }
                TraceLog(LOG_INFO, "[Migration] Task 15: Enemy files moved successfully.");
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_WARNING, "[Migration] Task 15 failed: %s", e.what());
                return false;
            }
        }
        else
        {
            TraceLog(LOG_INFO, "[Migration] Task 15: saves/enemies/ not found, skipping.");
        }
    }

    /*----------------------------------------------------------------------
     * Task 16: Pindahkan saves/items/ -> saves/slot_0/items/
     *----------------------------------------------------------------------*/
    {
        if (std::filesystem::exists("saves/items") && std::filesystem::is_directory("saves/items"))
        {
            EnsureSlotDirectory(0); // ensures saves/slot_0/items/ exists

            try
            {
                for (const auto& entry : std::filesystem::directory_iterator("saves/items"))
                {
                    if (entry.path().extension() == ".json")
                    {
                        std::string dest = "saves/slot_0/items/" + entry.path().filename().string();
                        std::filesystem::rename(entry.path(), dest);
                        TraceLog(LOG_INFO, "[Migration] Moved %s -> %s",
                                 entry.path().filename().string().c_str(), dest.c_str());
                    }
                }
                TraceLog(LOG_INFO, "[Migration] Task 16: Item files moved successfully.");
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_WARNING, "[Migration] Task 16 failed: %s", e.what());
                return false;
            }
        }
        else
        {
            TraceLog(LOG_INFO, "[Migration] Task 16: saves/items/ not found, skipping.");
        }
    }

    /*----------------------------------------------------------------------
     * Task 17: Hapus direktori lama + tulis sentinel
     *----------------------------------------------------------------------*/
    {
        try
        {
            // Hapus direktori flat lama
            if (std::filesystem::exists("saves/manual") && std::filesystem::is_directory("saves/manual"))
            {
                std::filesystem::remove_all("saves/manual");
                TraceLog(LOG_INFO, "[Migration] Removed saves/manual/");
            }
            if (std::filesystem::exists("saves/enemies") && std::filesystem::is_directory("saves/enemies"))
            {
                std::filesystem::remove_all("saves/enemies");
                TraceLog(LOG_INFO, "[Migration] Removed saves/enemies/");
            }
            if (std::filesystem::exists("saves/items") && std::filesystem::is_directory("saves/items"))
            {
                std::filesystem::remove_all("saves/items");
                TraceLog(LOG_INFO, "[Migration] Removed saves/items/");
            }

            // Tulis sentinel
            MarkMigrationComplete();

            TraceLog(LOG_INFO, "[Migration] v2->v3 migration completed successfully.");
            return true;
        }
        catch (const std::exception& e)
        {
            TraceLog(LOG_WARNING, "[Migration] Task 17 failed: %s", e.what());
            return false;
        }
    }
}

/*==============================================================================
 * State Save/Restore Functions
 *==============================================================================*/

/**
 * @brief SaveGameState()
 * Simpan seluruh state game world saat kembali ke menu.
 * @param state Pointer ke GameState
 * @note Serializes: player, enemies, items (with amount), map (dead entities, chestsOpened, bomb/crate consumedPositions, mapHistory)
 */
void SaveGameState(GameState *state)
{
    TraceLog(LOG_INFO, "Saving game state...");

    // Jalankan migrasi v2->v3 jika diperlukan (sebelum EnsureSlotDirectory)
    if (NeedsMigration())
    {
        bool migrated = RunMigration();
        if (!migrated)
        {
            TraceLog(LOG_WARNING, "[Migration] Pipeline failed, proceeding with normal save anyway.");
        }
    }

    // Pastikan direktori slot tersedia sebelum menulis
    if (g_ActiveSaveSlot >= 0)
        EnsureSlotDirectory(g_ActiveSaveSlot);

    /*==============================================================================
     * Save Player State
     *==============================================================================*/
    savedPlayerState.position = PlayerInstance.GetPosition();
    TraceLog(LOG_INFO, "SAVE: position = (%.2f, %.2f)", savedPlayerState.position.x, savedPlayerState.position.y);
    savedPlayerState.health = PlayerInstance.GetHealth();
    savedPlayerState.mana = PlayerInstance.GetMana();

    for (int i = 0; i < 4; i++)
    {
        savedPlayerState.hotbar[i] = PlayerInstance.GetHotbarItem(i);
    }

    // Save new fields: max stats, inventory bag, animation state, active slot
    savedPlayerState.maxHealth = PlayerInstance.MaxHealth;
    savedPlayerState.maxMana = PlayerInstance.MaxMana;

    for (int i = 0; i < 12; i++)
    {
        savedPlayerState.bag[i] = PlayerInstance.GetBagItem(i);
    }

    savedPlayerState.animState.state = PlayerInstance.Anim.state;
    savedPlayerState.animState.direction = PlayerInstance.Anim.direction;
    savedPlayerState.animState.isDead = PlayerInstance.Anim.isDead;
    savedPlayerState.animState.activeSlot = InputInstance.GetActiveSlot();

    // Save new player fields: dash cooldown, mana regen timer, attack state
    savedPlayerState.dashCooldown = PlayerInstance.DashCooldown;
    savedPlayerState.manaRegenTimer = PlayerInstance.ManaRegenTimer;
    savedPlayerState.swingAttack = {
        {"active", PlayerInstance.attack.active},
        {"timer", PlayerInstance.attack.timer},
        {"duration", PlayerInstance.attack.duration},
        {"raycastAngle", PlayerInstance.attack.raycastAngle},
        {"center", {PlayerInstance.attack.center.x, PlayerInstance.attack.center.y}},
        {"pressHeld", PlayerInstance.attack.pressHeld}
    };

    /*==============================================================================
     * Save Enemy States
     *==============================================================================*/
    savedEnemyStates.clear();
    auto &enemyReg = Entities::GetEnemyRegistry();
    for (const auto &enemy : enemyReg)
    {
        if (!enemy->IsActive) continue;
        SavedEnemyState saved;
        saved.position = enemy->Position;
        saved.enemyName = enemy->Name;
        saved.currentHP = (int)enemy->Health;
        saved.isAlive = enemy->IsAlive();
        saved.maxHealth = enemy->MaxHealth;
        saved.aiState = (int)enemy->AIState;
        saved.patrolTargetX = enemy->PatrolTarget.x;
        saved.patrolTargetY = enemy->PatrolTarget.y;
        saved.patrolTimer = enemy->PatrolTimer;
        saved.mapObjectID = enemy->MapObjectID;
        saved.spawnPoint = {{"x", enemy->SpawnPoint.x}, {"y", enemy->SpawnPoint.y}};
        saved.healthRegenTimer = enemy->HealthRegenTimer;
        saved.attackCooldownTimer = enemy->GetAttackCooldownTimer();
        saved.uuid = enemy->GetUUID();
        savedEnemyStates.push_back(saved);
    }

    /*==============================================================================
     * Save Item States
     *==============================================================================*/
    savedItemStates.clear();
    for (const ItemSpawn &item : itemData.activeItems)
    {
        SavedItemState savedItem;
        savedItem.position = item.position;
        savedItem.isPickedUp = item.isPickedUp;
        savedItem.definitionId = item.definitionId;
        savedItem.amount = item.amount;
        savedItem.uuid = item.uuid;
        savedItemStates.push_back(savedItem);
    }

    /*==============================================================================
     * Save Map State (map path, camera, chest opened status)
     *==============================================================================*/
    const char *mapPath = GetCurrentMapPath();
    savedMapState.mapPath = (mapPath == nullptr || mapPath[0] == '\0') ? "assets/maps/tutorial.json" : std::string(mapPath);
    savedPlayerState.mapDisplayName = GetMapDisplayName(savedMapState.mapPath);
    savedMapState.cameraTarget = camera.target;
    savedMapState.cameraZoom = camera.zoom;

    /*==============================================================================
     * Save Dead Entities (names of entities killed in this map)
     *==============================================================================*/
    savedMapState.deadEntities.clear();
    {
        const auto &deadSet = Entities::GetDeadEntities();
        savedMapState.deadEntities.assign(deadSet.begin(), deadSet.end());
    }

    /*==============================================================================
     * Save Consumed Chest Positions (chests already opened)
     *==============================================================================*/
    savedMapState.chestsOpened.clear();
    {
        const auto &consumed = chestManager.GetConsumedPositions();
        savedMapState.chestsOpened.assign(consumed.begin(), consumed.end());
    }

    /*==============================================================================
     * Save Consumed Bomb/Crate Positions
     *==============================================================================*/
    savedMapState.bombConsumedPositions = bombManager.GetConsumedPositions();
    savedMapState.crateConsumedPositions = crateManager.GetConsumedPositions();

    /*==============================================================================
     * Save Map History (stack of visited maps)
     *==============================================================================*/
    savedMapState.mapHistory = mapHistoryStack.GetAllEntries();

    TraceLog(LOG_INFO, "Game state saved (%d enemies, %d items)", (int)savedEnemyStates.size(), (int)savedItemStates.size());
}

/**
 * @brief RestoreGameState()
 * Kembalikan seluruh state game world saat masuk gameplay.
 * @param state Pointer ke GameState
 * @note Restores: player, enemies (with spawnPoint/timers/UUID), items (with amount), map (dead entities, chestsOpened, bomb/crate consumedPositions, mapHistory)
 */
void RestoreGameState(GameState *state)
{
    TraceLog(LOG_INFO, "Restoring game state...");
    /*==============================================================================
     * Restore Player State
     *==============================================================================*/
    if (hasSavedState)
    {
        // Restore max stats first so SetHealth/SetMana can clamp correctly
        if (savedPlayerState.maxHealth > 0)
        {
            PlayerInstance.MaxHealth = savedPlayerState.maxHealth;
            PlayerInstance.MaxMana = savedPlayerState.maxMana;
        }
        else
        {
            PlayerInstance.MaxHealth = 100.0f;
            PlayerInstance.MaxMana = 100.0f;
        }

        PlayerInstance.SetHealth(savedPlayerState.health);
        PlayerInstance.SetMana(savedPlayerState.mana);
        PlayerInstance.SetPosition(savedPlayerState.position);
        TraceLog(LOG_INFO, "RESTORE: SetPosition = (%.2f, %.2f)", savedPlayerState.position.x, savedPlayerState.position.y);

        for (int i = 0; i < 4; i++)
        {
            PlayerInstance.SetHotbarItem(i, savedPlayerState.hotbar[i]);
        }

        // Restore bag inventory
        for (int i = 0; i < 12; i++)
        {
            PlayerInstance.GetBagItem(i) = savedPlayerState.bag[i];
        }

        // Restore animation state
        PlayerInstance.Anim.state = static_cast<State>(savedPlayerState.animState.state);
        PlayerInstance.Anim.direction = static_cast<Direction>(savedPlayerState.animState.direction);
        PlayerInstance.Anim.isDead = savedPlayerState.animState.isDead;

        // Restore active slot
        InputInstance.SetActiveSlot(static_cast<ItemSlot>(savedPlayerState.animState.activeSlot));

        // Restore player combat/regen fields
        PlayerInstance.DashCooldown = savedPlayerState.dashCooldown;
        PlayerInstance.ManaRegenTimer = savedPlayerState.manaRegenTimer;

        // Restore attack state
        if (!savedPlayerState.swingAttack.is_null())
        {
            PlayerInstance.attack.active = savedPlayerState.swingAttack.value("active", false);
            PlayerInstance.attack.timer = savedPlayerState.swingAttack.value("timer", 0.0f);
            PlayerInstance.attack.duration = savedPlayerState.swingAttack.value("duration", 0.9f);
            PlayerInstance.attack.raycastAngle = savedPlayerState.swingAttack.value("raycastAngle", 0.0f);
            PlayerInstance.attack.pressHeld = savedPlayerState.swingAttack.value("pressHeld", false);
            if (savedPlayerState.swingAttack.contains("center"))
            {
                PlayerInstance.attack.center.x = savedPlayerState.swingAttack["center"][0].get<float>();
                PlayerInstance.attack.center.y = savedPlayerState.swingAttack["center"][1].get<float>();
            }
        }
    }

    /*==============================================================================
     * Restore Enemy States
     *==============================================================================*/
    if (hasSavedState && !savedEnemyStates.empty())
    {
        auto &enemyReg = Entities::GetEnemyRegistry();
        std::unordered_set<Enemy*> matchedEnemies;
        for (auto &saved : savedEnemyStates)
        {
            if (!saved.isAlive)
            {
                Entities::RegisterDeath(GetCurrentMapPath(), saved.mapObjectID);
                continue;
            }
            // First pass: match by UUID
            bool matched = false;
            for (auto &enemy : enemyReg)
            {
                if (enemy == nullptr || matchedEnemies.count(enemy)) continue;
                if (!saved.uuid.empty() && enemy->GetUUID() == saved.uuid)
                {
                    enemy->Position = saved.position;
                    enemy->Health = saved.currentHP;
                    enemy->MaxHealth = saved.maxHealth;
                    enemy->AIState = (EnemyAIState)(saved.aiState < 0 || saved.aiState > 4 ? 0 : saved.aiState);
                    enemy->PatrolTarget = {saved.patrolTargetX, saved.patrolTargetY};
                    enemy->PatrolTimer = saved.patrolTimer;
                    if (!saved.spawnPoint.is_null())
                    {
                        enemy->SpawnPoint.x = saved.spawnPoint["x"].get<float>();
                        enemy->SpawnPoint.y = saved.spawnPoint["y"].get<float>();
                    }
                    enemy->HealthRegenTimer = saved.healthRegenTimer;
                    // Grace: if timer is 0 and enemy is at full health, set to 2.0f to prevent instant regen after load
                    if (saved.healthRegenTimer <= 0.0f && enemy->Health >= enemy->MaxHealth)
                        enemy->HealthRegenTimer = 2.0f;
                    enemy->SetAttackCooldownTimer(saved.attackCooldownTimer);
                    enemy->IsActive = true;
                    matchedEnemies.insert(enemy);
                    matched = true;
                }
            }
            // Second pass: fallback to MapObjectID+Name matching (for legacy saves or dev migration)
            if (!matched)
            {
                for (auto &enemy : enemyReg)
                {
                    if (enemy == nullptr || matchedEnemies.count(enemy)) continue;
                    if (enemy->MapObjectID == saved.mapObjectID && enemy->Name == saved.enemyName)
                    {
                        enemy->Position = saved.position;
                        enemy->Health = saved.currentHP;
                        enemy->MaxHealth = saved.maxHealth;
                        enemy->AIState = (EnemyAIState)(saved.aiState < 0 || saved.aiState > 4 ? 0 : saved.aiState);
                        enemy->PatrolTarget = {saved.patrolTargetX, saved.patrolTargetY};
                        enemy->PatrolTimer = saved.patrolTimer;
                        if (!saved.spawnPoint.is_null())
                        {
                            enemy->SpawnPoint.x = saved.spawnPoint["x"].get<float>();
                            enemy->SpawnPoint.y = saved.spawnPoint["y"].get<float>();
                        }
                        enemy->HealthRegenTimer = saved.healthRegenTimer;
                        // Grace: if timer is 0 and enemy is at full health, set to 2.0f to prevent instant regen after load
                        if (saved.healthRegenTimer <= 0.0f && enemy->Health >= enemy->MaxHealth)
                            enemy->HealthRegenTimer = 2.0f;
                        enemy->SetAttackCooldownTimer(saved.attackCooldownTimer);
                        enemy->IsActive = true;
                        matchedEnemies.insert(enemy);
                        break;
                    }
                }
            }
        }
    }

    /*==============================================================================
     * Restore Item States
     *==============================================================================*/
    if (hasSavedState && !savedItemStates.empty())
    {
        // First pass: match by UUID
        for (const auto &saved : savedItemStates)
        {
            for (ItemSpawn &item : itemData.activeItems)
            {
                if (item.uuid == saved.uuid && !saved.uuid.empty())
                {
                    item.isPickedUp = saved.isPickedUp;
                    item.position = saved.position;
                    item.definitionId = saved.definitionId;
                    item.amount = saved.amount;
                    break;
                }
            }
        }
        // Second pass: fallback to index-based matching for items that were not matched by UUID
        int itemIndex = 0;
        for (ItemSpawn &item : itemData.activeItems)
        {
            if (itemIndex < (int)savedItemStates.size())
            {
                if (item.uuid.empty() || savedItemStates[itemIndex].uuid.empty() || item.uuid != savedItemStates[itemIndex].uuid)
                {
                    item.isPickedUp = savedItemStates[itemIndex].isPickedUp;
                    item.position = savedItemStates[itemIndex].position;
                    item.definitionId = savedItemStates[itemIndex].definitionId;
                    item.amount = savedItemStates[itemIndex].amount;
                }
                itemIndex++;
            }
        }
    }
    /*==============================================================================
     * Restore Map State (camera, chest opened status)
     *==============================================================================*/
    if (hasSavedState)
    {
        // Restore camera position
        camera.target = savedMapState.cameraTarget;
        camera.zoom = savedMapState.cameraZoom;

        // Fall back to tutorial map if saved map file is missing
        if (!std::filesystem::exists(savedMapState.mapPath))
        {
            TraceLog(LOG_WARNING, "Saved map not found: %s, falling back to assets/maps/tutorial.json", savedMapState.mapPath.c_str());
            savedMapState.mapPath = "assets/maps/tutorial.json";
        }

        // Restore consumed chest positions
        if (!savedMapState.chestsOpened.empty())
        {
            chestManager.SetConsumedPositions(std::unordered_set<std::string>(
                savedMapState.chestsOpened.begin(),
                savedMapState.chestsOpened.end()));
        }

        // Restore bomb/crate consumed positions
        bombManager.SetConsumedPositions(savedMapState.bombConsumedPositions.get<std::unordered_set<std::string>>());
        crateManager.SetConsumedPositions(savedMapState.crateConsumedPositions.get<std::unordered_set<std::string>>());

        // Restore map history
        if (!savedMapState.mapHistory.empty())
        {
            mapHistoryStack.FromVector(savedMapState.mapHistory);
        }
    }
    TraceLog(LOG_INFO, "Game state restored");
}

/** @brief Restore the DeadEntities set from savedMapState.
 *         Must be called BEFORE InitAll() to prevent dead enemies from respawning. */
void RestoreDeadEntities(void)
{
    if (!hasSavedState) return;
    if (!savedMapState.deadEntities.empty())
    {
        Entities::SetDeadEntities(std::set<std::string>(
            savedMapState.deadEntities.begin(),
            savedMapState.deadEntities.end()));
        TraceLog(LOG_INFO, "Restored %zu dead entities before entity spawn", savedMapState.deadEntities.size());
    }
}

/**
 * @brief HasSavedState()
 * Cek apakah ada state tersimpan.
 * @return true jika ada state yang bisa direstore
 */
bool HasSavedState(void)
{
    return hasSavedState;
}

/** @brief SetWorldgenPending()
 * Set atau clear flag worldgen pending.
 * Saat true, loading screen skip RestoreDeadEntities()
 * karena WorldgenIO::LoadRuntimeState yang handle.
 * @param pending true = worldgen load pending; false = normal flow
 */
void SetWorldgenPending(bool pending)
{
    worldgenPending = pending;
}

/** @brief IsWorldgenPending()
 * Check apakah ada worldgen stage load yang tertunda.
 * @return true jika loading screen harus skip RestoreDeadEntities
 */
bool IsWorldgenPending(void)
{
    return worldgenPending;
}

/**
 * @brief ResetMemoryState()
 * Reset semua state memory untuk fresh start — TIDAK menghapus worldseed directories.
 * Gunakan ResetWorldseed(slotIndex) secara terpisah jika perlu cleanup worldseed.
 */
void ResetMemoryState(void)
{
    hasSavedState = false;
    savedPlayerState.position = {0};
    savedPlayerState.health = 0;
    savedPlayerState.mana = 0;
    savedPlayerState.maxHealth = 0;
    savedPlayerState.maxMana = 0;
    for (int i = 0; i < 4; i++)
        savedPlayerState.hotbar[i] = {-1, 0};
    for (int i = 0; i < 12; i++)
        savedPlayerState.bag[i] = {-1, 0};
    savedPlayerState.animState = {0};
    savedPlayerState.dashCooldown = 0.0f;
    savedPlayerState.manaRegenTimer = 0.0f;
    savedPlayerState.swingAttack = nullptr;
    savedEnemyStates.clear();
    savedItemStates.clear();
    savedMapState.deadEntities.clear();
    savedMapState.chestsOpened.clear();
    savedMapState.mapHistory.clear();

    worldgenPending = false;

    // Reset SeedManager agar IsRunActive() false — tanpanya, New Game
    // kedua di worldgen skip InitRun() dan crash karena slot lama
    g_SeedManager.ResetRun();

    // Reset persistent world state for a fresh game
    Entities::ClearDeadEntities();
    chestManager.ResetConsumed();
    bombManager.ResetConsumed();
    crateManager.ResetConsumed();
}

/**
 * @brief ResetWorldseed()
 * Hapus folder worldseed/save_{slotIndex} untuk fresh worldgen start.
 * @param slotIndex Nomor slot yang akan dibersihkan (0-99)
 */
void ResetWorldseed(int slotIndex)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "assets/maps/World_generation/worldseed/save_%d", slotIndex);
    std::string path(buf);

    if (std::filesystem::exists(path))
    {
        std::filesystem::remove_all(path);
        TraceLog(LOG_INFO, "[ResetWorldseed] Removed worldgen save: %s", path.c_str());
    }
}
