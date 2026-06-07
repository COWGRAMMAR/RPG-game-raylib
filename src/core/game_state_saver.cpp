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
 * WriteAutosave - Simpan state game ke direktori saves/autosave/.
 * Membuat nama file dengan timestamp (autosave_DD-MM-YYYY-HH-MM-SS.json)
 * agar autosave tidak saling menimpa. Maksimal 5 file autosave —
 * jika lebih, file terlama akan dihapus.
 */
bool WriteAutosave(const std::string&)
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "autosave_%d-%m-%Y-%H-%M-%S.json", std::localtime(&t));
    std::string autosaveName(buf);
    TraceLog(LOG_INFO, "Autosaving to saves/autosave/%s", autosaveName.c_str());
    SaveGameState(gState);
    std::string dir = "saves/autosave";
    std::filesystem::create_directories(dir);

    bool result = WriteSaveFile(dir + "/" + autosaveName);

    // Batasi maksimal 5 slot autosave — hapus file terlama jika melebihi
    constexpr int MAX_AUTOSAVE_SLOTS = 5;
    std::vector<std::filesystem::path> autosaveFiles;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().filename().string().find("autosave_") == 0 && entry.path().extension() == ".json")
            autosaveFiles.push_back(entry.path());
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

        // Validate version
        int version = root.at("version").get<int>();
        if (version != SAVE_VERSION)
        {
            TraceLog(LOG_WARNING, "Save file version mismatch (expected %d): %s", SAVE_VERSION, path.c_str());
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
                enemy.healthRegenTimer = e.value("healthRegenTimer", 0.0f);
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
 * @brief ClearSavedState()
 * Bersihkan state tersimpan untuk fresh start.
 * Juga membersihkan folder worldseed/save_* untuk worldgen fresh start.
 */
void ClearSavedState(void)
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

    // Clean up stale worldseed save_N folders for a fresh worldgen start
    const char* worldseedDir = "assets/maps/World_generation/worldseed";
    if (std::filesystem::exists(worldseedDir))
    {
        for (auto& entry : std::filesystem::directory_iterator(worldseedDir))
        {
            if (entry.is_directory() && entry.path().string().find("save_") != std::string::npos)
            {
                std::filesystem::remove_all(entry.path());
                TraceLog(LOG_INFO, "[ClearSavedState] Removed stale worldgen save: %s", entry.path().string().c_str());
            }
        }
    }
}
