/**
 * @file savemanager.cpp
 * @brief Implementasi SaveManager — unified save/load orchestrator
 *
 * Implementasi dari GameSnapshot serialization, file I/O,
 * CaptureSnapshot, ApplyPreSpawn, dan ApplyPostSpawn.
 */

#include "savemanager.h"
#include "propsbehavior.h"
#include "entities.h"
#include "input.h"
#include "seedmanager.h"
#include "worldgenio.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <unordered_set>

namespace fs = std::filesystem;

using json = nlohmann::json;

/*==============================================================================
 * Path Helpers
 *==============================================================================*/

std::string SaveManager::GetSlotDir(int slot)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "saves/slot_%d", slot);
    return std::string(buf);
}

std::string SaveManager::GetManualPath(int slot)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "saves/slot_%d/manual/snapshot.json", slot);
    return std::string(buf);
}

std::string SaveManager::GetAutosaveDir(int slot)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "saves/slot_%d/autosave", slot);
    return std::string(buf);
}

std::string SaveManager::GetInitialPath(int slot)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "saves/slot_%d/manual/snapshot_initial.json", slot);
    return std::string(buf);
}

std::string SaveManager::SanitizePath(const std::string &mapPath)
{
    // Ganti karakter berbahaya jadi underscore
    std::string safe = mapPath;
    for (auto &c : safe)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '.' || c == ' ')
            c = '_';
    }
    return safe;
}

std::string SaveManager::GetCheckpointPath(const std::string &mapPath, int slot)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "saves/slot_%d/checkpoints/%s.json", slot, SanitizePath(mapPath).c_str());
    return std::string(buf);
}

/*==============================================================================
 * Directory Utilities
 *==============================================================================*/

bool SaveManager::EnsureDir(const std::string &dir)
{
    try
    {
        if (!fs::exists(dir))
            fs::create_directories(dir);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool SaveManager::EnsureDirs(int slot)
{
    std::string base = GetSlotDir(slot);
    return EnsureDir(base + "/manual") && EnsureDir(base + "/autosave") && EnsureDir(base + "/checkpoints");
}

/*==============================================================================
 * Atomic Write Helper
 *==============================================================================*/

bool SaveManager::AtomicWrite(const std::string &path, const json &data)
{
    std::string tmpPath = path + ".tmp";
    try
    {
        if (fs::exists(tmpPath))
            fs::remove(tmpPath);

        // Dump tanpa hash → hitung CRC32 → inject hash → dump ulang.
        // nlohmann::json pake std::map (key di-sort alphabetically), jadi output dump(4) deterministic.
        std::string content = data.dump(4);
        unsigned int crc = ComputeCRC32(
            reinterpret_cast<unsigned char *>(content.data()),
            static_cast<int>(content.size()));

        json dataWithHash = data;
        dataWithHash["hash"] = crc;
        std::string finalContent = dataWithHash.dump(4);

        std::ofstream file(tmpPath);
        if (!file.is_open())
            return false;
        file << finalContent;
        file.close();

        fs::rename(tmpPath, path);
        return fs::exists(path);
    }
    catch (...)
    {
        return false;
    }
}

void SaveManager::CleanupTmpFiles()
{
    if (!fs::exists("saves"))
        return;
    try
    {
        for (auto &entry : fs::recursive_directory_iterator("saves"))
        {
            if (entry.path().extension() == ".tmp")
                fs::remove(entry.path());
        }
    }
    catch (...)
    {
    }
}

/*==============================================================================
 * Serialize / Deserialize
 *==============================================================================*/

json SaveManager::Serialize(const GameSnapshot &snap)
{
    json root;

    // Meta
    root["version"] = GameSnapshot::SNAPSHOT_VERSION;
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&t));
    root["timestamp"] = std::string(buf);
    root["slotIndex"] = snap.slotIndex;
    root["worldgenSlot"] = snap.worldgenSlot;
    root["stageIndex"] = snap.stageIndex;

    // Player
    json playerJson;
    playerJson["position"] = {snap.playerPosition.x, snap.playerPosition.y};
    playerJson["health"] = snap.playerHealth;
    playerJson["maxHealth"] = snap.playerMaxHealth;
    playerJson["mana"] = snap.playerMana;
    playerJson["maxMana"] = snap.playerMaxMana;

    json hotbarJson = json::array();
    for (int i = 0; i < 4; i++)
    {
        json item;
        item["definitionId"] = snap.hotbar[i].definitionId;
        item["amount"] = snap.hotbar[i].amount;
        hotbarJson.push_back(item);
    }
    playerJson["hotbar"] = hotbarJson;

    json bagJson = json::array();
    for (int i = 0; i < 12; i++)
    {
        json item;
        item["definitionId"] = snap.bag[i].definitionId;
        item["amount"] = snap.bag[i].amount;
        bagJson.push_back(item);
    }
    playerJson["bag"] = bagJson;

    json animJson;
    animJson["state"] = snap.animState.state;
    animJson["direction"] = snap.animState.direction;
    animJson["isDead"] = snap.animState.isDead;
    animJson["activeSlot"] = snap.animState.activeSlot;
    playerJson["animState"] = animJson;

    playerJson["showFPS"] = snap.showFPS;
    playerJson["dashCooldown"] = snap.dashCooldown;
    playerJson["manaRegenTimer"] = snap.manaRegenTimer;
    playerJson["swingAttack"] = snap.swingAttack;
    root["player"] = playerJson;

    // Enemies
    json enemiesJson = json::array();
    for (const auto &enemy : snap.enemies)
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

    // Items
    json itemsJson = json::array();
    for (const auto &item : snap.items)
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

    // Props — chests
    {
        json arr = json::array();
        for (const auto &pos : snap.chestConsumed)
            arr.push_back(pos);
        root["chestsOpened"] = arr;
    }

    // Props — bombs & crates
    root["bombConsumedPositions"] = json(snap.bombConsumed);
    root["crateConsumedPositions"] = json(snap.crateConsumed);

    // Props — barrierMap (persistent truth per map)
    json barrierJson = json::object();
    for (const auto &[path, cleared] : snap.barrierMap)
        barrierJson[path] = cleared;
    root["barrierMap"] = barrierJson;

    // Dead Entities
    {
        json arr = json::array();
        for (const auto &d : snap.deadEntities)
            arr.push_back(d);
        root["deadEntities"] = arr;
    }

    // Map
    json mapJson;
    mapJson["mapPath"] = snap.mapPath;
    mapJson["cameraTarget"] = {snap.cameraTarget.x, snap.cameraTarget.y};
    mapJson["cameraZoom"] = snap.cameraZoom;
    mapJson["mapDisplayName"] = snap.mapDisplayName;

    json historyJson = json::array();
    for (const auto &entry : snap.mapHistory)
    {
        json h;
        h["mapPath"] = entry.mapPath;
        h["doorName"] = entry.doorName;
        historyJson.push_back(h);
    }
    mapJson["mapHistory"] = historyJson;
    root["map"] = mapJson;

    return root;
}

GameSnapshot SaveManager::Deserialize(const json &root)
{
    GameSnapshot snap;
    snap.version = root.value("version", -1);
    snap.slotIndex = root.value("slotIndex", -1);
    snap.worldgenSlot = root.value("worldgenSlot", -1);
    snap.stageIndex = root.value("stageIndex", -1);

    // Player
    if (root.contains("player"))
    {
        const auto &player = root.at("player");
        snap.playerPosition.x = player.at("position")[0].get<float>();
        snap.playerPosition.y = player.at("position")[1].get<float>();
        snap.playerHealth = player.value("health", 100.0f);
        snap.playerMaxHealth = player.value("maxHealth", 100.0f);
        snap.playerMana = player.value("mana", 100.0f);
        snap.playerMaxMana = player.value("maxMana", 100.0f);

        if (player.contains("hotbar"))
        {
            const auto &hotbar = player.at("hotbar");
            for (int i = 0; i < HOTBAR_SLOTS && i < (int)hotbar.size(); i++)
            {
                snap.hotbar[i].definitionId = hotbar[i].value("definitionId", -1);
                snap.hotbar[i].amount = hotbar[i].value("amount", 0);
            }
        }

        if (player.contains("bag"))
        {
            const auto &bag = player.at("bag");
            for (int i = 0; i < BAG_SLOTS && i < (int)bag.size(); i++)
            {
                snap.bag[i].definitionId = bag[i].value("definitionId", -1);
                snap.bag[i].amount = bag[i].value("amount", 0);
            }
        }

        if (player.contains("animState"))
        {
            const auto &anim = player.at("animState");
            snap.animState.state = anim.value("state", 0);
            snap.animState.direction = anim.value("direction", 0);
            snap.animState.isDead = anim.value("isDead", false);
            snap.animState.activeSlot = anim.value("activeSlot", 0);
        }

        snap.showFPS = player.value("showFPS", false);
        snap.dashCooldown = player.value("dashCooldown", 0.0f);
        snap.manaRegenTimer = player.value("manaRegenTimer", 0.0f);
        if (player.contains("swingAttack"))
            snap.swingAttack = player.at("swingAttack");
    }

    // Enemies
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
            enemy.spawnPoint = e.value("spawnPoint", json({{"x", 0}, {"y", 0}}));
            enemy.healthRegenTimer = e.value("healthRegenTimer", 2.0f);
            enemy.attackCooldownTimer = e.value("attackCooldownTimer", 0.0f);
            enemy.uuid = e.value("uuid", "");
            snap.enemies.push_back(enemy);
        }
    }

    // Items
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
            snap.items.push_back(item);
        }
    }

    // Props — chests
    if (root.contains("chestsOpened"))
    {
        for (const auto &c : root.at("chestsOpened"))
            snap.chestConsumed.insert(c.get<std::string>());
    }

    // Props — bombs & crates
    if (root.contains("bombConsumedPositions"))
    {
        for (const auto &b : root.at("bombConsumedPositions"))
            snap.bombConsumed.insert(b.get<std::string>());
    }
    if (root.contains("crateConsumedPositions"))
    {
        for (const auto &c : root.at("crateConsumedPositions"))
            snap.crateConsumed.insert(c.get<std::string>());
    }

    // Props — barrierMap (persistent truth per map)
    if (root.contains("barrierMap"))
    {
        for (auto &[path, cleared] : root["barrierMap"].items())
            snap.barrierMap[path] = cleared.get<bool>();
    }

    // Dead Entities
    if (root.contains("deadEntities"))
    {
        for (const auto &d : root.at("deadEntities"))
            snap.deadEntities.insert(d.get<std::string>());
    }

    // Map
    if (root.contains("map"))
    {
        const auto &map = root.at("map");
        snap.mapPath = map.value("mapPath", "");
        if (map.contains("cameraTarget") && map["cameraTarget"].is_array() && map["cameraTarget"].size() >= 2)
        {
            snap.cameraTarget.x = map["cameraTarget"][0].get<float>();
            snap.cameraTarget.y = map["cameraTarget"][1].get<float>();
        }
        snap.cameraZoom = map.value("cameraZoom", 1.0f);
        snap.mapDisplayName = map.value("mapDisplayName", "");

        if (map.contains("mapHistory"))
        {
            for (const auto &h : map.at("mapHistory"))
            {
                MapSystem::MapHistoryEntry entry;
                entry.mapPath = h.value("mapPath", "");
                entry.doorName = h.value("doorName", "");
                snap.mapHistory.push_back(entry);
            }
        }
    }

    return snap;
}

/*==============================================================================
 * Full Save / Load
 *==============================================================================*/

bool SaveManager::WriteSnapshot(const GameSnapshot &snap, const std::string &path)
{
    TraceLog(LOG_INFO, "[SaveManager] Writing snapshot to: %s", path.c_str());
    json root = Serialize(snap);
    return AtomicWrite(path, root);
}

GameSnapshot SaveManager::ReadSnapshot(const std::string &path)
{
    TraceLog(LOG_INFO, "[SaveManager] Reading snapshot from: %s", path.c_str());
    if (!fs::exists(path))
    {
        TraceLog(LOG_WARNING, "[SaveManager] Snapshot not found: %s", path.c_str());
        return GameSnapshot();
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

            // Re-serialize without hash — must produce same string as pre-hash dump
            std::string content = root.dump(4);
            unsigned int computedHash = ComputeCRC32(
                reinterpret_cast<unsigned char *>(content.data()),
                static_cast<int>(content.size()));

            if (computedHash != storedHash)
            {
                TraceLog(LOG_WARNING, "[SaveManager] CRC32 mismatch (computed=%u stored=%u): %s",
                         computedHash, storedHash, path.c_str());
                return GameSnapshot();
            }
        }

        if (!root.contains("version"))
        {
            TraceLog(LOG_WARNING, "[SaveManager] Invalid snapshot (no version): %s", path.c_str());
            return GameSnapshot();
        }

        int version = root.at("version").get<int>();
        if (version != GameSnapshot::SNAPSHOT_VERSION)
        {
            TraceLog(LOG_WARNING, "[SaveManager] Version mismatch (expected %d, got %d): %s",
                     GameSnapshot::SNAPSHOT_VERSION, version, path.c_str());
            return GameSnapshot();
        }

        return Deserialize(root);
    }
    catch (const json::parse_error &e)
    {
        TraceLog(LOG_WARNING, "[SaveManager] Parse error: %s", e.what());
        return GameSnapshot();
    }
    catch (const json::out_of_range &e)
    {
        TraceLog(LOG_WARNING, "[SaveManager] Missing field: %s", e.what());
        return GameSnapshot();
    }
    catch (const json::type_error &e)
    {
        TraceLog(LOG_WARNING, "[SaveManager] Type error: %s", e.what());
        return GameSnapshot();
    }
}

bool SaveManager::HasSnapshot(const std::string &path)
{
    return fs::exists(path) && fs::file_size(path) > 0;
}

/*==============================================================================
 * Convenience
 *==============================================================================*/

/*=== Manual Save ===*/

bool SaveManager::SaveManual(const GameSnapshot &snap, int slot)
{
    if (slot < 0) return false;

    // 1. Bersihkan workspace manual (snapshot.json + snapshot_initial.json)
    //    Autosave DIKEEP — biar ikut di-copy ke slot via CopyWorkspaceTo()
    ClearWorkspaceManual();

    // 2. Generate fresh initial snapshot
    CaptureInitialSnapshot(-1);

    // 3. Write snapshot + current checkpoint ke -1
    if (!WriteSnapshot(snap, GetManualPath(-1)))
        return false;
    if (!snap.mapPath.empty())
        SaveCheckpoint(snap, snap.mapPath, -1);

    // 4. Mirror seluruh workspace ke slot tujuan
    CopyWorkspaceTo(slot);

    return true;
}

GameSnapshot SaveManager::LoadManual(int slot)
{
    return ReadSnapshot(GetManualPath(slot));
}

bool SaveManager::HasManual(int slot)
{
    return HasSnapshot(GetManualPath(slot));
}

bool SaveManager::SaveAutosave(int slot)
{
    EnsureDirs(slot);

    // Generate timestamped filename
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "snapshot_%d-%m-%Y-%H-%M-%S.json", std::localtime(&t));
    std::string filename(buf);
    std::string dir = GetAutosaveDir(slot);

    // Capture & write
    GameSnapshot snap = CaptureSnapshot();
    if (!WriteSnapshot(snap, dir + "/" + filename))
        return false;

    // Prune ke max 5 file
    constexpr int MAX_AUTOSAVE = 5;
    std::vector<fs::path> files;
    if (fs::exists(dir))
    {
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() == ".json" && entry.path().filename().string().find("snapshot_") == 0)
                files.push_back(entry.path());
        }
    }
    if (files.size() > MAX_AUTOSAVE)
    {
        std::sort(files.begin(), files.end(),
                  [](const auto &a, const auto &b)
                  {
                      return fs::last_write_time(a) < fs::last_write_time(b);
                  });
        for (size_t i = 0; i < files.size() - MAX_AUTOSAVE; i++)
            fs::remove(files[i]);
    }

    return true;
}

/*==============================================================================
 * Per-map Checkpoint
 *==============================================================================*/

bool SaveManager::SaveCheckpoint(const GameSnapshot &snap, const std::string &mapPath, int slot)
{
    EnsureDirs(slot);
    return WriteSnapshot(snap, GetCheckpointPath(mapPath, slot));
}

GameSnapshot SaveManager::LoadCheckpoint(const std::string &mapPath, int slot)
{
    return ReadSnapshot(GetCheckpointPath(mapPath, slot));
}

bool SaveManager::HasCheckpoint(const std::string &mapPath, int slot)
{
    return HasSnapshot(GetCheckpointPath(mapPath, slot));
}

/*==============================================================================
 * Initial Snapshot (Restart)
 *==============================================================================*/

bool SaveManager::SaveInitial(const GameSnapshot &snap, int slot)
{
    EnsureDirs(slot);
    return WriteSnapshot(snap, GetInitialPath(slot));
}

GameSnapshot SaveManager::LoadInitial(int slot)
{
    return ReadSnapshot(GetInitialPath(slot));
}

bool SaveManager::HasInitial(int slot)
{
    return HasSnapshot(GetInitialPath(slot));
}

bool SaveManager::MirrorToWorkspace(int sourceSlot)
{
    if (sourceSlot < 0) return false;
    constexpr int WORKSPACE = -1;

    EnsureDirs(WORKSPACE);

    std::string srcDir = GetSlotDir(sourceSlot);
    std::string dstDir = GetSlotDir(WORKSPACE);

    try
    {
        // Bersihin dulu, baru copy — biar gak ada data basi dari slot lain
        ClearWorkspaceManual();
        ClearWorkspaceAutosave();
        ClearWorkspaceCheckpoints();

        auto CopyDir = [&](const std::string &subdir)
        {
            std::string src = srcDir + "/" + subdir;
            std::string dst = dstDir + "/" + subdir;
            if (!fs::exists(src) || !fs::is_directory(src))
                return;
            fs::create_directories(dst);
            for (const auto &entry : fs::directory_iterator(src))
            {
                if (entry.path().extension() == ".json")
                {
                    fs::copy(entry.path(),
                             fs::path(dst) / entry.path().filename(),
                             fs::copy_options::overwrite_existing);
                }
            }
        };

        CopyDir("checkpoints");
        CopyDir("manual");
        CopyDir("autosave");

        return true;
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "MirrorToWorkspace(%d) failed: %s", sourceSlot, e.what());
        return false;
    }
}

void SaveManager::ClearWorkspaceCheckpoints()
{
    constexpr int WORKSPACE = -1;
    std::string chkDir = GetSlotDir(WORKSPACE) + "/checkpoints";

    try
    {
        if (fs::exists(chkDir) && fs::is_directory(chkDir))
        {
            for (const auto &entry : fs::directory_iterator(chkDir))
            {
                if (entry.path().extension() == ".json")
                {
                    fs::remove(entry.path());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "ClearWorkspaceCheckpoints: %s", e.what());
    }
}

void SaveManager::ClearWorkspaceManual()
{
    constexpr int WORKSPACE = -1;
    std::string dir = GetSlotDir(WORKSPACE) + "/manual";

    try
    {
        if (fs::exists(dir))
            fs::remove_all(dir);
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "ClearWorkspaceManual: %s", e.what());
    }
}

void SaveManager::ClearWorkspaceAutosave()
{
    constexpr int WORKSPACE = -1;
    std::string dir = GetSlotDir(WORKSPACE) + "/autosave";

    try
    {
        if (fs::exists(dir))
            fs::remove_all(dir);
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "ClearWorkspaceAutosave: %s", e.what());
    }
}

void SaveManager::CopyWorkspaceTo(int slot)
{
    if (slot < 0) return;
    EnsureDirs(slot);

    auto copyDir = [&](const std::string &subdir)
    {
        constexpr int WORKSPACE = -1;
        std::string src = GetSlotDir(WORKSPACE) + "/" + subdir;
        std::string dst = GetSlotDir(slot) + "/" + subdir;

        if (!fs::exists(src)) return;

        try
        {
            fs::create_directories(dst);
            for (const auto &entry : fs::directory_iterator(src))
            {
                if (entry.path().extension() == ".json")
                {
                    fs::copy(entry.path(),
                             fs::path(dst) / entry.path().filename(),
                             fs::copy_options::overwrite_existing);
                }
            }
        }
        catch (const std::exception &e)
        {
            TraceLog(LOG_WARNING, "CopyWorkspaceTo: copy %s failed: %s",
                     subdir.c_str(), e.what());
        }
    };

    copyDir("checkpoints");
    copyDir("manual");
    copyDir("autosave");
}

/*==============================================================================
 * Capture Snapshot (read live state)
 *==============================================================================*/

GameSnapshot SaveManager::CaptureSnapshot()
{
    GameSnapshot snap;
    snap.version = GameSnapshot::SNAPSHOT_VERSION;
    snap.slotIndex = g_ActiveSaveSlot;

    // Worldgen slot
    snap.worldgenSlot = g_SeedManager.IsRunActive() ? g_SeedManager.GetCurrentSlot() : -1;

    // Player
    snap.playerPosition = PlayerInstance.GetPosition();
    snap.playerHealth = PlayerInstance.GetHealth();
    snap.playerMana = PlayerInstance.GetMana();
    snap.playerMaxHealth = PlayerInstance.MaxHealth;
    snap.playerMaxMana = PlayerInstance.MaxMana;

    for (int i = 0; i < 4; i++)
        snap.hotbar[i] = PlayerInstance.GetHotbarItem(i);
    for (int i = 0; i < 12; i++)
        snap.bag[i] = PlayerInstance.GetBagItem(i);

    snap.animState.state = PlayerInstance.Anim.state;
    snap.animState.direction = PlayerInstance.Anim.direction;
    snap.animState.isDead = PlayerInstance.Anim.isDead;
    snap.animState.activeSlot = InputInstance.GetActiveSlot();

    snap.dashCooldown = PlayerInstance.DashCooldown;
    snap.manaRegenTimer = PlayerInstance.ManaRegenTimer;
    snap.swingAttack = {
        {"active", PlayerInstance.attack.active},
        {"timer", PlayerInstance.attack.timer},
        {"duration", PlayerInstance.attack.duration},
        {"raycastAngle", PlayerInstance.attack.raycastAngle},
        {"center", {PlayerInstance.attack.center.x, PlayerInstance.attack.center.y}},
        {"pressHeld", PlayerInstance.attack.pressHeld}};

    if (gState)
        snap.showFPS = gState->showFPS;

    // Enemies
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
        snap.enemies.push_back(saved);
    }

    // Items
    for (const ItemSpawn &item : itemData.activeItems)
    {
        SavedItemState savedItem;
        savedItem.position = item.position;
        savedItem.isPickedUp = item.isPickedUp;
        savedItem.definitionId = item.definitionId;
        savedItem.amount = item.amount;
        savedItem.uuid = item.uuid;
        snap.items.push_back(savedItem);
    }

    // Props — chests
    snap.chestConsumed = chestManager.GetConsumedPositions();

    // Props — bombs & crates
    snap.bombConsumed = bombManager.GetConsumedPositions();
    snap.crateConsumed = crateManager.GetConsumedPositions();

    // Props — barrierMap (simpan status clearance map saat ini)
    {
        const char *mapPath = GetCurrentMapPath();
        if (mapPath && mapPath[0] != '\0')
            snap.barrierMap[std::string(mapPath)] = barrierManager.IsCleared();
    }

    // Dead Entities
    {
        const auto &deadSet = Entities::GetDeadEntities();
        snap.deadEntities = deadSet;
    }

    // Map
    const char *mapPath = GetCurrentMapPath();
    snap.mapPath = (mapPath && mapPath[0] != '\0') ? std::string(mapPath) : "assets/maps/main_hub.json";
    snap.mapDisplayName = GetMapDisplayName(snap.mapPath);
    snap.cameraTarget = camera.target;
    snap.cameraZoom = camera.zoom;
    snap.mapHistory = mapHistoryStack.GetAllEntries();
    snap.version = GameSnapshot::SNAPSHOT_VERSION;

    return snap;
}

bool SaveManager::CaptureInitialSnapshot(int slot)
{
    GameSnapshot snap = CaptureSnapshot();
    return SaveInitial(snap, slot);
}

/*==============================================================================
 * Apply Pre-Spawn (BEFORE InitAll / SpawnEnemiesFromMap / SpawnObject)
 *==============================================================================*/

void SaveManager::ApplyPreSpawn(const GameSnapshot &snap)
{
    if (snap.version != GameSnapshot::SNAPSHOT_VERSION)
        return;

    // Dead enemies handled per-instance by ApplyPostSpawn, not per-spawn-point here
    /*--- Props: chest consumed (biar SpawnObject skip yg udah diambil) ---*/
    if (!snap.chestConsumed.empty())
        chestManager.SetConsumedPositions(snap.chestConsumed);

    /*--- Props: bomb consumed ---*/
    if (!snap.bombConsumed.empty())
        bombManager.SetConsumedPositions(snap.bombConsumed);

    /*--- Props: crate consumed ---*/
    if (!snap.crateConsumed.empty())
        crateManager.SetConsumedPositions(snap.crateConsumed);

    /*--- Props: barrier state ---*/
    {
        bool barrierCleared = false;
        const char *mapPath = GetCurrentMapPath();
        if (mapPath && mapPath[0] != '\0')
        {
            auto it = snap.barrierMap.find(std::string(mapPath));
            if (it != snap.barrierMap.end())
                barrierCleared = it->second;
        }
        barrierManager.SetCleared(barrierCleared);
    }
}

/*==============================================================================
 * Apply Post-Spawn (AFTER InitAll / SpawnEnemiesFromMap / SpawnObject)
 *==============================================================================*/

void SaveManager::ApplyPostSpawn(const GameSnapshot &snap)
{
    // Skip if snapshot is empty (version check)
    if (snap.version != GameSnapshot::SNAPSHOT_VERSION)
        return;

    TraceLog(LOG_INFO, "[SaveManager] Applying post-spawn state...");

    /*--- Player ---*/
    if (snap.playerMaxHealth > 0)
    {
        PlayerInstance.MaxHealth = snap.playerMaxHealth;
        PlayerInstance.MaxMana = snap.playerMaxMana;
    }
    else
    {
        PlayerInstance.MaxHealth = 100.0f;
        PlayerInstance.MaxMana = 100.0f;
    }

    PlayerInstance.SetHealth(snap.playerHealth);
    PlayerInstance.SetMana(snap.playerMana);
    PlayerInstance.SetPosition(snap.playerPosition);

    for (int i = 0; i < 4; i++)
        PlayerInstance.SetHotbarItem(i, snap.hotbar[i]);
    for (int i = 0; i < 12; i++)
        PlayerInstance.GetBagItem(i) = snap.bag[i];

    PlayerInstance.Anim.state = static_cast<State>(snap.animState.state);
    PlayerInstance.Anim.direction = static_cast<Direction>(snap.animState.direction);
    PlayerInstance.Anim.isDead = snap.animState.isDead;
    InputInstance.SetActiveSlot(static_cast<ItemSlot>(snap.animState.activeSlot));

    PlayerInstance.DashCooldown = snap.dashCooldown;
    PlayerInstance.ManaRegenTimer = snap.manaRegenTimer;

    if (!snap.swingAttack.is_null())
    {
        PlayerInstance.attack.active = snap.swingAttack.value("active", false);
        PlayerInstance.attack.timer = snap.swingAttack.value("timer", 0.0f);
        PlayerInstance.attack.duration = snap.swingAttack.value("duration", 0.9f);
        PlayerInstance.attack.raycastAngle = snap.swingAttack.value("raycastAngle", 0.0f);
        PlayerInstance.attack.pressHeld = snap.swingAttack.value("pressHeld", false);
        if (snap.swingAttack.contains("center"))
        {
            PlayerInstance.attack.center.x = snap.swingAttack["center"][0].get<float>();
            PlayerInstance.attack.center.y = snap.swingAttack["center"][1].get<float>();
        }
    }

    /*--- Enemies ---*/
    if (!snap.enemies.empty())
    {
        auto &enemyReg = Entities::GetEnemyRegistry();
        std::unordered_set<Enemy *> matchedEnemies;

        for (const auto &saved : snap.enemies)
        {
            if (!saved.isAlive)
            {
                // Deactivate matched enemy directly; RegisterDeath would poison the shared MapObjectID
                for (auto &enemy : enemyReg)
                {
                    if (!enemy || matchedEnemies.count(enemy))
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
                if (!enemy || matchedEnemies.count(enemy))
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
                    if (saved.healthRegenTimer <= 0.0f && enemy->Health >= enemy->MaxHealth)
                        enemy->HealthRegenTimer = 2.0f;
                    enemy->SetAttackCooldownTimer(saved.attackCooldownTimer);
                    enemy->IsActive = true;
                    matchedEnemies.insert(enemy);
                    matched = true;
                    break;
                }
            }

            // Second pass: fallback to MapObjectID+Name
            if (!matched)
            {
                for (auto &enemy : enemyReg)
                {
                    if (!enemy || matchedEnemies.count(enemy))
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

    /*--- Items (full replacement - snapshot is source of truth) ---*/
    itemData.activeItems.clear();
    for (const auto &saved : snap.items)
    {
        ItemSpawn item;
        item.position = saved.position;
        item.isPickedUp = saved.isPickedUp;
        item.definitionId = saved.definitionId;
        item.amount = saved.amount;
        item.uuid = saved.uuid;
        // Rekonstruksi hitbox dari definisi item agar magnet/pickup berfungsi normal
        {
            const ItemDefinition &def = itemDefs.GetById(item.definitionId);
            float halfW = def.hitboxSize.x / 2.0f;
            float halfH = def.hitboxSize.y / 2.0f;
            item.hitbox = {item.position.x - halfW,
                           item.position.y - halfH,
                           def.hitboxSize.x,
                           def.hitboxSize.y};
        }
        item.spawnTime = (float)GetTime(); // Immunity mulai dari sekarang
        item.isAdded = item.isPickedUp;
        itemData.activeItems.push_back(item);
    }

    /*--- Props: chest consumed ---*/
    if (!snap.chestConsumed.empty())
        chestManager.SetConsumedPositions(snap.chestConsumed);

    /*--- Props: bomb consumed ---*/
    if (!snap.bombConsumed.empty())
        bombManager.SetConsumedPositions(snap.bombConsumed);

    /*--- Props: crate consumed ---*/
    if (!snap.crateConsumed.empty())
        crateManager.SetConsumedPositions(snap.crateConsumed);

    /*--- Props: barrier state ---*/
    {
        bool barrierCleared = false;
        const char *mapPath = GetCurrentMapPath();
        if (mapPath && mapPath[0] != '\0')
        {
            auto it = snap.barrierMap.find(std::string(mapPath));
            if (it != snap.barrierMap.end())
                barrierCleared = it->second;
        }
        if (barrierCleared)
            barrierManager.RemoveAllBarriers();
        else
            barrierManager.SetCleared(false);
    }

    /*--- Map: camera ---*/
    camera.target = snap.cameraTarget;
    camera.zoom = snap.cameraZoom > 0.0f ? snap.cameraZoom : 1.0f;

    /*--- Map: history ---*/
    if (!snap.mapHistory.empty())
        mapHistoryStack.FromVector(snap.mapHistory);

    TraceLog(LOG_INFO, "[SaveManager] Post-spawn restore complete (%zu enemies, %zu items)",
             snap.enemies.size(), snap.items.size());
}

/*=== ApplyCheckpointData ===*/

/**
 * @brief Apply checkpoint/cache state — partial restore
 *
 * Restore enemies, items, props (chest/bomb/crate, barrier).
 * TIDAK restore player, camera, mapHistory.
 * ApplyPreSpawn() HARUS dipanggil SEBELUM fungsi ini.
 */
void SaveManager::ApplyCheckpointData(const GameSnapshot &snap)
{
    if (snap.version != GameSnapshot::SNAPSHOT_VERSION)
        return;

    TraceLog(LOG_INFO, "[SaveManager] Applying checkpoint data...");

    /*--- Enemies ---*/
    if (!snap.enemies.empty())
    {
        auto &enemyReg = Entities::GetEnemyRegistry();
        std::unordered_set<Enemy *> matchedEnemies;

        for (const auto &saved : snap.enemies)
        {
            if (!saved.isAlive)
            {
                // Deactivate matched enemy directly; RegisterDeath would poison the shared MapObjectID
                for (auto &enemy : enemyReg)
                {
                    if (!enemy || matchedEnemies.count(enemy))
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

            bool matched = false;
            for (auto &enemy : enemyReg)
            {
                if (!enemy || matchedEnemies.count(enemy))
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
                    if (saved.healthRegenTimer <= 0.0f && enemy->Health >= enemy->MaxHealth)
                        enemy->HealthRegenTimer = 2.0f;
                    enemy->SetAttackCooldownTimer(saved.attackCooldownTimer);
                    enemy->IsActive = true;
                    matchedEnemies.insert(enemy);
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                for (auto &enemy : enemyReg)
                {
                    if (!enemy || matchedEnemies.count(enemy))
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

    /*--- Items (full replacement - snapshot is source of truth) ---*/
    if (!snap.items.empty())
    {
        itemData.activeItems.clear();
        for (const auto &saved : snap.items)
        {
            ItemSpawn item;
            item.position = saved.position;
            item.isPickedUp = saved.isPickedUp;
            item.definitionId = saved.definitionId;
            item.amount = saved.amount;
            item.uuid = saved.uuid;
            // Rekonstruksi hitbox dari definisi item agar magnet/pickup berfungsi normal
            {
                const ItemDefinition &def = itemDefs.GetById(item.definitionId);
                float halfW = def.hitboxSize.x / 2.0f;
                float halfH = def.hitboxSize.y / 2.0f;
                item.hitbox = {item.position.x - halfW,
                               item.position.y - halfH,
                               def.hitboxSize.x,
                               def.hitboxSize.y};
            }
            item.spawnTime = (float)GetTime(); // Immunity mulai dari sekarang
            item.isAdded = item.isPickedUp;
            itemData.activeItems.push_back(item);
        }
    }

    /*--- Props: chest consumed ---*/
    if (!snap.chestConsumed.empty())
        chestManager.SetConsumedPositions(snap.chestConsumed);

    /*--- Props: bomb consumed ---*/
    if (!snap.bombConsumed.empty())
        bombManager.SetConsumedPositions(snap.bombConsumed);

    /*--- Props: crate consumed ---*/
    if (!snap.crateConsumed.empty())
        crateManager.SetConsumedPositions(snap.crateConsumed);

    /*--- Props: barrier state ---*/
    {
        bool barrierCleared = false;
        const char *mapPath = GetCurrentMapPath();
        if (mapPath && mapPath[0] != '\0')
        {
            auto it = snap.barrierMap.find(std::string(mapPath));
            if (it != snap.barrierMap.end())
                barrierCleared = it->second;
        }
        if (barrierCleared)
            barrierManager.RemoveAllBarriers();
        else
            barrierManager.SetCleared(false);
    }

    TraceLog(LOG_INFO, "[SaveManager] Checkpoint restore complete (%zu enemies, %zu items)",
             snap.enemies.size(), snap.items.size());
}

/*==============================================================================
 * Slot Management
 *==============================================================================*/

bool SaveManager::DeleteSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex > 11)
    {
        TraceLog(LOG_WARNING, "[SaveManager] Invalid slot index: %d", slotIndex);
        return false;
    }

    std::string slotDir = GetSlotDir(slotIndex);
    if (!fs::exists(slotDir))
    {
        TraceLog(LOG_INFO, "[SaveManager] Slot %d already empty", slotIndex);
        return true;
    }

    try
    {
        fs::remove_all(slotDir);
        TraceLog(LOG_INFO, "[SaveManager] Deleted %s", slotDir.c_str());
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "[SaveManager] Failed to delete slot %d: %s", slotIndex, e.what());
        return false;
    }

    // Bersihin worldseed orphan
    WorldgenIO::CleanupOrphanedSlots();

    // Reset active slot kalo yang kehapus adalah slot aktif
    if (g_ActiveSaveSlot == slotIndex)
        SetActiveSlot(-1);

    return true;
}
