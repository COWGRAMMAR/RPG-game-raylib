/**
 * @file game_state_saver.cpp
 * @brief Implementasi Modul Preservasi State Game
 *
 * Handle save dan restore seluruh state game world.
 */

#include "game_state_saver.h"
#include "savemanager.h"
#include "seedmanager.h"
#include "map.h"
#include "propsbehavior.h"
#include "entities.h"
#include "worldgenio.h"
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
std::string GetSlotPath(int slot, const std::string &type)
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
 *          Tidak melakukan apa-apa jika direktori sudah ada.
 */
void EnsureSlotDirectory(int slot)
{
    const char *subdirs[] = {"manual", "autosave"};
    for (const char *subdir : subdirs)
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
bool WriteSaveFile(const std::string &path)
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
    root["slotIndex"] = (g_ActiveSaveSlot >= 0) ? g_ActiveSaveSlot : -1; ///< -1 = unassigned, 0-4 = manual slot
    root["saveType"] = "manual";                                         ///< "manual" or "autosave"
    root["playTime"] = 0.0f;                                             ///< Placeholder: gameplay timer deferred
    root["mapDisplayName"] = savedPlayerState.mapDisplayName;
    root["worldgenSlot"] = g_SeedManager.IsRunActive() ? g_SeedManager.GetCurrentSlot() : -1; ///< -1 = unassigned, maps to worldseed/save_N/

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
    for (const auto &enemy : savedEnemyStates)
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
    for (const auto &item : savedItemStates)
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
    for (const auto &name : savedMapState.deadEntities)
        deadEntitiesJson.push_back(name);
    mapJson["deadEntities"] = deadEntitiesJson;

    json chestsOpenedJson = json::array();
    for (const auto &pos : savedMapState.chestsOpened)
        chestsOpenedJson.push_back(pos);
    mapJson["chestsOpened"] = chestsOpenedJson;

    json mapHistoryJson = json::array();
    for (const auto &entry : savedMapState.mapHistory)
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

    // Atomic write: dump tanpa hash → hitung CRC32 → inject hash → dump ulang
    std::string tmpPath = path + ".tmp";
    if (std::filesystem::exists(tmpPath))
        std::filesystem::remove(tmpPath);

    std::string content = root.dump(4);
    unsigned int crc = ComputeCRC32(
        reinterpret_cast<unsigned char *>(content.data()),
        static_cast<int>(content.size()));

    json rootWithHash = root;
    rootWithHash["hash"] = crc;
    std::string finalContent = rootWithHash.dump(4);

    std::ofstream file(tmpPath);
    file << finalContent;
    file.close();

    std::filesystem::rename(tmpPath, path);

    bool saveResult = std::filesystem::exists(path);
    TraceLog(LOG_INFO, "Save to %s: %s", path.c_str(), saveResult ? "SUCCESS" : "FAILED");
    return saveResult;
}

/**
 * ReadSaveFile - Read JSON save file and deserialize into global saved state.
 */
bool ReadSaveFile(const std::string &path)
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

        // Verifikasi CRC32 hash kalo ada (backward-compat: save lama tanpa hash skip check)
        auto hashIt = root.find("hash");
        if (hashIt != root.end())
        {
            unsigned int storedHash = hashIt->get<unsigned int>();
            root.erase(hashIt);

            std::string content = root.dump(4);
            unsigned int computedHash = ComputeCRC32(
                reinterpret_cast<unsigned char *>(content.data()),
                static_cast<int>(content.size()));

            if (computedHash != storedHash)
            {
                TraceLog(LOG_WARNING, "Save file CRC32 mismatch (computed=%u stored=%u): %s",
                         computedHash, storedHash, path.c_str());
                return false;
            }
        }

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
        const auto &player = root.at("player");
        savedPlayerState.position.x = player.at("position")[0].get<float>();
        savedPlayerState.position.y = player.at("position")[1].get<float>();
        TraceLog(LOG_INFO, "LOAD: read position = (%.2f, %.2f) from %s", savedPlayerState.position.x, savedPlayerState.position.y, path.c_str());
        savedPlayerState.health = player.value("health", 100.0f);
        savedPlayerState.maxHealth = player.value("maxHealth", 100.0f);
        savedPlayerState.mana = player.value("mana", 100.0f);
        savedPlayerState.maxMana = player.value("maxMana", 100.0f);

        if (player.contains("hotbar"))
        {
            const auto &hotbar = player.at("hotbar");
            for (int i = 0; i < HOTBAR_SLOTS && i < (int)hotbar.size(); i++)
            {
                savedPlayerState.hotbar[i].definitionId = hotbar[i].value("definitionId", -1);
                savedPlayerState.hotbar[i].amount = hotbar[i].value("amount", 0);
            }
        }

        if (player.contains("bag"))
        {
            const auto &bag = player.at("bag");
            for (int i = 0; i < BAG_SLOTS && i < (int)bag.size(); i++)
            {
                savedPlayerState.bag[i].definitionId = bag[i].value("definitionId", -1);
                savedPlayerState.bag[i].amount = bag[i].value("amount", 0);
            }
        }

        if (player.contains("animState"))
        {
            const auto &anim = player.at("animState");
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
            for (const auto &e : root.at("enemies"))
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
            for (const auto &it : root.at("items"))
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
        const auto &map = root.at("map");
        savedMapState.mapPath = map.value("mapPath", "");
        savedMapState.cameraTarget.x = map.at("cameraTarget")[0].get<float>();
        savedMapState.cameraTarget.y = map.at("cameraTarget")[1].get<float>();
        savedMapState.cameraZoom = map.value("cameraZoom", 1.0f);

        savedMapState.deadEntities.clear();
        if (map.contains("deadEntities"))
        {
            for (const auto &d : map.at("deadEntities"))
                savedMapState.deadEntities.push_back(d.get<std::string>());
        }

        savedMapState.chestsOpened.clear();
        if (map.contains("chestsOpened"))
        {
            for (const auto &c : map.at("chestsOpened"))
                savedMapState.chestsOpened.push_back(c.get<std::string>());
        }

        savedMapState.mapHistory.clear();
        if (map.contains("mapHistory"))
        {
            for (const auto &h : map.at("mapHistory"))
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
    catch (const json::parse_error &)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - parse error: %s", path.c_str());
        return false;
    }
    catch (const json::out_of_range &)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - missing field: %s", path.c_str());
        return false;
    }
    catch (const json::type_error &)
    {
        TraceLog(LOG_WARNING, "Save file corrupted - type error: %s", path.c_str());
        return false;
    }
}

/**
 * DeleteSaveFile - Remove save file if it exists.
 */
void DeleteSaveFile(const std::string &path)
{
    TraceLog(LOG_INFO, "Deleting save file: %s", path.c_str());
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
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

    // === OLD PATH: keep globals for backward compat (WriteSaveFile/WriteAutosave) ===

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
        {"pressHeld", PlayerInstance.attack.pressHeld}};

    /*==============================================================================
     * Save Enemy States
     *==============================================================================*/
    savedEnemyStates.clear();
    auto &enemyReg = Entities::GetEnemyRegistry();
    for (const auto &enemy : enemyReg)
    {
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
    savedMapState.mapPath = (mapPath == nullptr || mapPath[0] == '\0') ? "assets/maps/main_hub.json" : std::string(mapPath);
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
    TraceLog(LOG_INFO, "RestoreGameState: slot=%d hasSaved=%d enemies=%zu items=%zu mapPath='%s'",
             g_ActiveSaveSlot, hasSavedState, savedEnemyStates.size(), savedItemStates.size(), savedMapState.mapPath.c_str());

    // === NEW PATH: SaveManager (preferred) ===
    if (g_ActiveSaveSlot >= 0 && SaveManager::HasManual(g_ActiveSaveSlot))
    {
        GameSnapshot snap = SaveManager::LoadManual(g_ActiveSaveSlot);
        TraceLog(LOG_INFO, "RestoreGameState: snapshot version=%d (expected=%d) enemies=%zu items=%zu playerPos=(%.0f,%.0f) mapPath='%s' worldgenSlot=%d",
                 snap.version, GameSnapshot::SNAPSHOT_VERSION,
                 snap.enemies.size(), snap.items.size(),
                 snap.playerPosition.x, snap.playerPosition.y,
                 snap.mapPath.c_str(), snap.worldgenSlot);
        if (snap.version == GameSnapshot::SNAPSHOT_VERSION)
        {
            SaveManager::ApplyPostSpawn(snap);
            TraceLog(LOG_INFO, "RestoreGameState: restored via SaveManager (slot %d)", g_ActiveSaveSlot);
            return;
        }
        TraceLog(LOG_WARNING, "RestoreGameState: snapshot version mismatch (%d != %d), falling back to old format",
                 snap.version, GameSnapshot::SNAPSHOT_VERSION);
    }
    else if (g_ActiveSaveSlot >= 0)
    {
        TraceLog(LOG_INFO, "RestoreGameState: no new-format snapshot for slot %d, trying old-format fallback", g_ActiveSaveSlot);
    }

    // === OLD PATH FALLBACK: restore dari global state ===

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
            PlayerInstance.MaxHealth = DEFAULT_MAX_HEALTH;
            PlayerInstance.MaxMana = DEFAULT_MAX_MANA;
        }

        PlayerInstance.SetHealth(savedPlayerState.health);
        PlayerInstance.SetMana(savedPlayerState.mana);
        PlayerInstance.SetPosition(savedPlayerState.position);
        TraceLog(LOG_INFO, "RESTORE: SetPosition = (%.2f, %.2f)", savedPlayerState.position.x, savedPlayerState.position.y);

        for (int i = 0; i < HOTBAR_SLOTS; i++)
        {
            PlayerInstance.SetHotbarItem(i, savedPlayerState.hotbar[i]);
        }

        // Restore bag inventory
        for (int i = 0; i < BAG_SLOTS; i++)
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
        std::unordered_set<Enemy *> matchedEnemies;
        for (auto &saved : savedEnemyStates)
        {
            if (!saved.isAlive)
            {
                // Deactivate matched enemy directly (same pattern as ApplyPostSpawn);
                // RegisterDeath no longer prevents SpawnEnemiesFromMap from spawning
                for (auto &enemy : enemyReg)
                {
                    if (enemy == nullptr || matchedEnemies.count(enemy))
                        continue;
                    if (enemy->MapObjectID == saved.mapObjectID && enemy->Name == saved.enemyName)
                    {
                        enemy->IsActive = false;
                        enemy->Health = 0.0f;
                        matchedEnemies.insert(enemy);
                        break;
                    }
                }
                continue;
            }
            // First pass: match by UUID
            bool matched = false;
            for (auto &enemy : enemyReg)
            {
                if (enemy == nullptr || matchedEnemies.count(enemy))
                    continue;
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
                    if (enemy == nullptr || matchedEnemies.count(enemy))
                        continue;
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
                    item.isAdded = saved.isPickedUp;
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
                    item.isAdded = savedItemStates[itemIndex].isPickedUp;
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

        // Fall back to main hub if saved map file is missing
        if (!std::filesystem::exists(savedMapState.mapPath))
        {
            TraceLog(LOG_WARNING, "Saved map not found: %s, falling back to assets/maps/main_hub.json", savedMapState.mapPath.c_str());
            savedMapState.mapPath = "assets/maps/main_hub.json";
        }

        // Restore consumed chest positions
        if (!savedMapState.chestsOpened.empty())
        {
            chestManager.SetConsumedPositions(std::unordered_set<std::string>(
                savedMapState.chestsOpened.begin(),
                savedMapState.chestsOpened.end()));
        }

        // Restore bomb/crate consumed positions (guarded against null/non-array JSON)
        if (!savedMapState.bombConsumedPositions.is_null() && savedMapState.bombConsumedPositions.is_array())
            bombManager.SetConsumedPositions(savedMapState.bombConsumedPositions.get<std::unordered_set<std::string>>());
        if (!savedMapState.crateConsumedPositions.is_null() && savedMapState.crateConsumedPositions.is_array())
            crateManager.SetConsumedPositions(savedMapState.crateConsumedPositions.get<std::unordered_set<std::string>>());

        // Restore map history
        if (!savedMapState.mapHistory.empty())
        {
            mapHistoryStack.FromVector(savedMapState.mapHistory);
        }
    }
    TraceLog(LOG_INFO, "Game state restored");
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
