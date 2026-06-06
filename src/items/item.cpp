/**
 * @file item.cpp
 * @brief Implementasi Item System
 *
 * File ini berisi implementasi untuk sistem item di dungeon:
 * - ItemDataManager: manajemen data item aktif, spawn, save/load per map
 * - ItemRenderManager: update magnet, pickup detection, dan render item
 * - ItemSpawnManager: baca spawn area dari Tiled, kategorisasi, dan spawn acak per run
 *
 * Free functions dipertahankan sebagai wrapper untuk backward compatibility.
 */

#include "item.h"
// include fonts.h untuk fontLoadingTitle
#include "fonts.h"
#include "screen.h"
#include "inventory.h"
#include "combat.h"
#include "player.h"
#include "animation.h"
#include "screen.h"
#include "entities.h"
#include "enemy.h"
#include "mapLogic.h"
#include "datadriven.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include "../lib/raylib/include/raymath.h"
#include "core/utils.h"
#include "core/game_state_saver.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <stdexcept>

using json = nlohmann::json;
using namespace DataDriven;

// instance global keempat class
/** @brief Instance global definisi item */
ItemDefinitionManager itemDefs;
/** @brief Instance global data item */
ItemDataManager itemData;
/** @brief Instance global render item */
ItemRenderManager itemRender;
/** @brief Instance global spawn manager */
ItemSpawnManager spawnManager;

/*==============================================================================
 * Free functions (backward compat)
 *==============================================================================*/

/**
 * @brief Inisialisasi seluruh item system
 *
 * Load texture, init data manager, load spawn area dari Tiled, spawn item pertama kali.
 */
void InitItems()
{
    itemDefs.Load("assets/data/items.json");
    itemData.Init();
    spawnManager.Init(ITEM_LAYER_NAME);
    spawnManager.SpawnAll(itemData.activeItems);
}

/**
 * @brief Reset dan spawn ulang semua item (untuk new run atau reload map)
 *
 * Clear activeItems, re-init spawn area, dan spawn ulang.
 */
void SpawnItemWave()
{
    itemData.activeItems.clear();
    spawnManager.Init(ITEM_LAYER_NAME);
    spawnManager.SpawnAll(itemData.activeItems);
}

/** @brief Spawn item tambahan tanpa clear activeItems */
void SpawnRandomItem()
{
    spawnManager.SpawnAll(itemData.activeItems);
}

/** @brief Getter untuk activeItems */
std::vector<ItemSpawn> &GetActiveItems() { return itemData.activeItems; }

/** @brief Drop semua item player ke ground dengan spread offset */
void DropAllItems(Player &player)
{
    constexpr int SPREAD = 40;
    constexpr int EMPTY_ID = -1;

    auto DropSlot = [&](InventoryItem &slot)
    {
        if (slot.definitionId == EMPTY_ID || slot.amount <= 0)
            return;
        Vector2 dropPos = {
            player.Position.x + (float)GetRandomValue(-SPREAD, SPREAD),
            player.Position.y + (float)GetRandomValue(-SPREAD, SPREAD)};
        ItemSpawn item = itemData.CreateItem(dropPos, slot.definitionId);
        item.amount = slot.amount;
        itemData.activeItems.push_back(item);
        slot = {EMPTY_ID, 0};
    };

    for (int i = 0; i < player.GetMaxHotbar(); i++)
        DropSlot(player.GetHotbarItem(i));
    for (int i = 0; i < player.GetMaxBag(); i++)
        DropSlot(player.GetBagItem(i));

    player.hasDroppedItems = true;
}

/*==============================================================================
 * ItemDefinitionManager
 *==============================================================================*/

/**
 * @brief Memuat semua definisi item dari file JSON.
 * @param path Path ke file JSON (contoh: "assets/data/items.json")
 * @throws std::runtime_error Jika file tidak bisa dibuka
 */
void ItemDefinitionManager::Load(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open: " + path);

    json root = json::parse(file);

    for (auto &[name, data] : root.at("items").items())
    {
        ItemDefinition def;
        def.id = SafeGet<int>(data, "id", -1);
        def.name = SafeGet<std::string>(data, "name", name);
        def.spriteKey = SafeGet<std::string>(data, "spriteKey", name);
        if (data.contains("sheetCoord"))
        {
            def.sheetCoord = ParseVector2(data.at("sheetCoord"));
        }
        else
        {
            def.sheetCoord = Vector2{0, 0};
        }
        def.hitboxSize = ParseVector2(data.at("hitboxSize"));
        def.isStackable = SafeGet<bool>(data, "isStackable", false); // nilai fallback false
        def.maxStack = SafeGet<int>(data, "maxStack", 1);            // nilai fallback 1

        // Parse category
        std::string cat = SafeGet<std::string>(data, "category", "none"); // nilai fallback none
        if (cat == "weapon")
            def.category = ITEM_WEAPON;
        else if (cat == "potion")
            def.category = ITEM_POTION;
        else if (cat == "poison")
            def.category = ITEM_POISON;
        else if (cat == "armor")
            def.category = ITEM_ARMOR;
        else
            def.category = ITEM_NONE;

        // Parse rarity
        std::string rar = SafeGet<std::string>(data, "rarity", "common"); // nilai fallback common
        if (rar == "common")
            def.rarity = RARITY_COMMON;
        else if (rar == "uncommon")
            def.rarity = RARITY_UNCOMMON;
        else if (rar == "rare")
            def.rarity = RARITY_RARE;
        else if (rar == "epic")
            def.rarity = RARITY_EPIC;

        // Parse variant berdasarkan category
        if (def.category == ITEM_WEAPON)
        {
            const auto &w = data.at("weapon");
            WeaponData wd;
            wd.damage = SafeGet<float>(w, "damage", 10.f);                    // nilai fallback 10
            wd.reach = SafeGet<float>(w, "reach", 10.f);                      // nilai fallback 10
            wd.breadth = SafeGet<float>(w, "breadth", 10.f);                  // nilai fallback 10
            wd.duration = SafeGet<float>(w, "duration", 0.9f);                // nilai fallback 0.9
            wd.knockbackForce = SafeGet<float>(w, "knockbackForce", 1.f);     // nilai fallback 1
            wd.startAngleOffset = SafeGet<float>(w, "startAngleOffset", 0.f); // nilai fallback 0
            wd.sweepAngle = SafeGet<float>(w, "sweepAngle", 0.f);             // nilai fallback 0
            wd.centerOffset = ParseVector2(w.at("centerOffset"));
            wd.manaCost = SafeGet<float>(w, "manaCost", 25.f); // nilai fallback 25

            // parse attack type
            std::string at = SafeGet<std::string>(w, "attackType", "thrust"); // nilai fallback thrust
            if (at == "slash")
                wd.attackType = ATTACK_SLASH;
            else if (at == "thrust")
                wd.attackType = ATTACK_THRUST;
            else if (at == "pierce")
                wd.attackType = ATTACK_PIERCE;
            else if (at == "slam")
                wd.attackType = ATTACK_SLAM;

            def.data = wd;
        }
        else if (def.category == ITEM_POTION || def.category == ITEM_POISON)
        {
            // ITEM_POISON berbagi struktur data yang sama dengan ITEM_POTION
            const auto &p = data.at("potion");
            PotionData pd;
            pd.healValue = SafeGet<int>(p, "healValue", 0); // nilai fallback 0
            pd.isMana = SafeGet<bool>(p, "isMana", false);  // nilai fallback false
            def.data = pd;
        }

        // ini belum di impementasikan
        else if (def.category == ITEM_ARMOR)
        {
            const auto &a = data.at("armor");
            ArmorData ad;
            ad.defense = SafeGet<float>(a, "defense", 0.f); // nilai fallback 0
            def.data = ad;
        }

        definitions_[name] = std::move(def);
    }

    // Populasi index ID numerik untuk O(1) lookup
    for (auto &[name, def] : definitions_)
        byId_[def.id] = &def;
}

/**
 * @brief Mencari definisi item berdasarkan ID numerik.
 * @param id ID item yang dicari
 * @return Referensi ke ItemDefinition yang cocok
 * @throws std::runtime_error Jika ID tidak ditemukan
 */
const ItemDefinition &ItemDefinitionManager::GetById(int id) const
{
    auto it = byId_.find(id);
    if (it != byId_.end())
        return *it->second;
    throw std::runtime_error("ItemDefinition not found for id: " + std::to_string(id));
}

/**
 * @brief Mengambil semua definisi item.
 * @return Referensi ke map keseluruhan definisi item
 */
const std::unordered_map<std::string, ItemDefinition> &ItemDefinitionManager::GetAll() const
{
    return definitions_;
}

Vector2 ItemDefinitionManager::GetMaxHitboxForCategory(ItemCategory category) const
{
    Vector2 maxSize = {0, 0};
    for (const auto &[name, def] : definitions_)
    {
        if (category != ITEM_ANY && def.category != category)
            continue;
        if (def.hitboxSize.x > maxSize.x)
            maxSize.x = def.hitboxSize.x;
        if (def.hitboxSize.y > maxSize.y)
            maxSize.y = def.hitboxSize.y;
    }
    return maxSize;
}

/*==============================================================================
 * ItemDataSpawnManager
 *==============================================================================*/

/** @brief Reset activeItems */
void ItemDataManager::Init()
{
    activeItems.clear();
}

/**
 * @brief Buat item baru dengan properti lengkap
 *
 * Set nama, hitbox, dan rarity berdasarkan category.
 * Log spawn ke console.
 *
 * @param pos Posisi spawn item
 * @param category Jenis item (ITEM_WEAPON / ITEM_POTION)
 * @param multiplier Stat multiplier item
 * @param rarity Rarity item
 * @return Item yang sudah di-setup
 */
ItemSpawn ItemDataManager::CreateItem(Vector2 pos, int definitionId)
{
    const ItemDefinition &def = itemDefs.GetById(definitionId);

    // cari posisi aman, maksimal `spawnPosRetryLimit` attempt
    int spawnPosRetryLimit = 5;
    Vector2 safePos = pos;
    float halfW = def.hitboxSize.x / 2.0f;
    float halfH = def.hitboxSize.y / 2.0f;
    for (int i = 0; i < spawnPosRetryLimit; i++)
    {
        // Convert center→top-left karena IsPositionSafe expect top-left
        Vector2 topLeft = {safePos.x - halfW, safePos.y - halfH};
        if (IsPositionSafe(topLeft, def.hitboxSize.x, def.hitboxSize.y, 0, 0))
            break;
        safePos = {
            pos.x + (float)GetRandomValue(-32, 32),
            pos.y + (float)GetRandomValue(-32, 32)};
    }

    ItemSpawn item;
    item.definitionId = definitionId;
    item.position = safePos;
    item.hitbox = {safePos.x - halfW,
                   safePos.y - halfH,
                   def.hitboxSize.x,
                   def.hitboxSize.y};
    item.isPickedUp = false;
    item.isAdded = false;
    item.spawnTime = (float)GetTime();

    item.uuid = GenerateUUID();

    TraceLog(LOG_INFO, "ITEM: Spawned '%s' at (%.1f, %.1f)",
             def.name.c_str(), safePos.x, safePos.y);
    return item;
}

/**
 * @brief Spawn item secara langsung ke posisi tertentu
 *
 * Dipakai oleh ChestManager saat chest dibuka.
 * Category dipilih random antara ITEM_POTION dan ITEM_WEAPON.
 *
 * @param pos Posisi spawn item
 */
void ItemDataManager::SpawnItemAtLocation(Vector2 pos, std::mt19937 *rng, ItemCategory category)
{
    std::mt19937 localRng(static_cast<unsigned int>(time(nullptr)));
    std::mt19937 &useRng = rng ? *rng : localRng;
    int defId = spawnManager.PickRandomDefinitionId(useRng, category);
    if (defId == -1)
        return;
    activeItems.push_back(CreateItem(pos, defId));
}

/**
 * @brief Simpan state activeItems untuk map tertentu
 * @param mapPath Path map sebagai key penyimpanan
 */
void ItemDataManager::SaveItemsForMap(const std::string &mapPath)
{
    if (mapPath.empty())
        return;
    savedMapItems[mapPath] = activeItems;
}

/**
 * @brief Load state activeItems yang tersimpan untuk map tertentu
 * @param mapPath Path map sebagai key pencarian
 * @return true jika data ditemukan dan di-load, false jika belum ada
 */
bool ItemDataManager::LoadItemsForMap(const std::string &mapPath)
{
    if (mapPath.empty())
        return false;
    auto it = savedMapItems.find(mapPath);
    if (it != savedMapItems.end())
    {
        activeItems = it->second;
        return true;
    }
    return false;
}

/** @brief Hapus semua item aktif */
void ItemDataManager::ClearItems()
{
    activeItems.clear();
}

/*==============================================================================
 * Per-Map File Persistence
 *==============================================================================*/

/**
 * @brief Simpan semua item aktif untuk suatu map ke filesystem per-slot save.
 *
 * Menyimpan tiap field item (definitionId, position, isPickedUp, amount, uuid)
 * ke array JSON dengan key "items". Menggunakan atomic write (.tmp + rename).
 *
 * @param mapPath Path map mentah untuk menurunkan nama file save
 * @param baseDir Direktori dasar (default diganti ke path per-slot otomatis)
 */
void SaveItemsForMapDir(const std::string &mapPath, const std::string &baseDir)
{
    // Sanitize map path: replace path separators with underscores
    std::string safeName = mapPath;
    for (auto &c : safeName)
    {
        if (c == '/' || c == '\\') c = '_';
    }

    // Gunakan direktori per-slot jika baseDir adalah default
    std::string dir = baseDir;
    if (dir == "saves/items")
    {
        dir = "saves/slot_" + std::to_string(g_ActiveSaveSlot) + "/items";
        EnsureSlotDirectory(g_ActiveSaveSlot);
    }

    std::string filePath = dir + "/" + safeName;

    std::filesystem::create_directories(dir);

    json root;
    json itemsJson = json::array();

    for (const auto &item : itemData.activeItems)
    {
        json i;
        i["definitionId"] = item.definitionId;
        i["positionX"] = item.position.x;
        i["positionY"] = item.position.y;
        i["isPickedUp"] = item.isPickedUp;
        i["amount"] = item.amount;
        i["uuid"] = item.uuid;
        itemsJson.push_back(i);
    }

    root["items"] = itemsJson;

    // Atomic write via .tmp + rename to prevent file corruption
    std::string tmpPath = filePath + ".tmp";
    std::ofstream file(tmpPath);
    file << root.dump(4);
    file.close();
    std::filesystem::rename(tmpPath, filePath);

    TraceLog(LOG_INFO, "SAVE: Saved %d items for map: %s", (int)itemData.activeItems.size(), mapPath.c_str());
}

/**
 * @brief Muat item untuk suatu map dari filesystem per-slot save.
 *
 * Membaca file JSON, deserialiasi tiap item, rekonstruksi hitbox dari
 * data ItemDefinition, dan mengisi itemData.activeItems.
 *
 * @param mapPath Path map mentah untuk menurunkan nama file save
 * @param baseDir Direktori dasar (default diganti ke path per-slot otomatis)
 * @return true jika item berhasil dimuat, false jika file tidak ada atau gagal parse
 */
bool LoadItemsForMapDir(const std::string &mapPath, const std::string &baseDir)
{
    // Sanitize map path: replace path separators with underscores
    std::string safeName = mapPath;
    for (auto &c : safeName)
    {
        if (c == '/' || c == '\\') c = '_';
    }

    // Gunakan direktori per-slot jika baseDir adalah default
    std::string dir = baseDir;
    if (dir == "saves/items")
    {
        dir = "saves/slot_" + std::to_string(g_ActiveSaveSlot) + "/items";
    }

    std::string filePath = dir + "/" + safeName;

    if (!std::filesystem::exists(filePath))
        return false;

    try
    {
        std::ifstream file(filePath);
        json root = json::parse(file);

        if (!root.contains("items"))
            return false;

        itemData.activeItems.clear();

        for (const auto &i : root.at("items"))
        {
            ItemSpawn item;
            item.definitionId = i.value("definitionId", -1);
            item.position = {
                i.value("positionX", 0.0f),
                i.value("positionY", 0.0f)
            };
            item.isPickedUp = i.value("isPickedUp", false);
            item.amount = i.value("amount", 1);
            item.uuid = i.value("uuid", "");

            // Reconstruct hitbox from definition data
            try
            {
                const ItemDefinition &def = itemDefs.GetById(item.definitionId);
                item.hitbox = {
                    item.position.x - def.hitboxSize.x / 2,
                    item.position.y - def.hitboxSize.y / 2,
                    def.hitboxSize.x,
                    def.hitboxSize.y
                };
            }
            catch (...)
            {
                // Fallback hitbox if definition is not found
                item.hitbox = {
                    item.position.x - 8.0f,
                    item.position.y - 8.0f,
                    16.0f,
                    16.0f
                };
            }

            item.isAdded = false;
            item.spawnTime = (float)GetTime();

            itemData.activeItems.push_back(item);
        }

        TraceLog(LOG_INFO, "LOAD: Restored %d items for map: %s", (int)itemData.activeItems.size(), mapPath.c_str());
        return !itemData.activeItems.empty();
    }
    catch (...)
    {
        TraceLog(LOG_WARNING, "LOAD: Failed to load items for map: %s", mapPath.c_str());
        return false;
    }
}

/*==============================================================================
 * ItemRenderManager
 *==============================================================================*/

/**
 * @brief Update magnet effect dan pickup detection tiap frame
 *
 * Item yang dalam magnetRadius akan bergerak ke arah player.
 * Item yang overlap dengan playerHitbox akan di-pickup (isPickedUp = true).
 * Item yang baru spawn (< 1 detik) diabaikan.
 *
 * @param items Referensi ke activeItems
 * @param playerCenter Posisi center player untuk magnet
 * @param playerHitbox Hitbox player untuk pickup
 * @param magnetRadius Radius magnet dalam pixel
 * @param itemSpeed Kecepatan gerak item saat ditarik magnet
 */
void ItemRenderManager::Update(std::vector<ItemSpawn> &items, Vector2 playerCenter,
                               Rectangle playerHitbox, float magnetRadius, float itemSpeed)
{

    float spawnImmunityDuration = 1.0f;
    float currentTime = (float)GetTime();
    for (auto &item : items)
    {
        if (item.isPickedUp)
            continue;
        if (currentTime - item.spawnTime < spawnImmunityDuration)
            continue;
        Vector2 itemCenter = {
            item.hitbox.x + item.hitbox.width / 2,
            item.hitbox.y + item.hitbox.height / 2};

        float dist = Vector2Distance(playerCenter, itemCenter);
        if (dist <= magnetRadius && Inventory::HasInventorySpace(PlayerInstance))
        {
            Vector2 dir = Vector2Normalize(Vector2Subtract(playerCenter, itemCenter));
            item.position.x += dir.x * itemSpeed * Time::DELTA_TIME;
            item.position.y += dir.y * itemSpeed * Time::DELTA_TIME;
            item.hitbox.x = item.position.x;
            item.hitbox.y = item.position.y;
        }

        if (CheckCollisionRecs(playerHitbox, item.hitbox))
        {
            item.isPickedUp = true;
            TraceLog(LOG_INFO, "ITEM: Picked up '%s'",
                     itemDefs.GetById(item.definitionId).name.c_str());
        }
    }
}

/**
 * @brief Render semua item yang belum di-pickup
 * @param items Referensi ke activeItems
 */
int ItemRenderManager::RenderAll(std::vector<ItemSpawn> &items, Rectangle viewRect)
{
    int rendered = 0;
    for (auto &item : items)
    {
        if (!item.isPickedUp && CheckCollisionRecs(item.hitbox, viewRect))
        {
            Render(item);
            rendered++;
        }
    }
    return rendered;
}

/**
 * @brief Render satu item berdasarkan category
 *
 * ITEM_POTION = sheet coord (7,8), ITEM_WEAPON = sheet coord (6,4).
 *
 * @param item Item yang akan di-render
 */
void ItemRenderManager::Render(ItemSpawn &item)
{
    Vector2 center = {
        item.hitbox.x + item.hitbox.width / 2,
        item.hitbox.y + item.hitbox.height / 2};
    const float scale = 1.0f;
    Vector2 renderPos = {
        center.x - 16.0f,
        center.y - 16.0f};

    const ItemDefinition &def = itemDefs.GetById(item.definitionId);

    Display display;
    display.position = renderPos;
    display.size = (int)(FRAME_SIZE * scale);
    DrawFrame(def.spriteKey, display);

    // stack amount item di-drop: fontLoadingTitle 14px, bg rounded hitam, di bawah sprite
    if (item.amount > 1)
    {
        std::string amountText = std::to_string(item.amount);
        int fontSize = 14;
        Vector2 textSz = MeasureTextEx(fontLoadingTitle, amountText.c_str(), fontSize, 0);
        Vector2 textPos = {
            center.x - textSz.x / 2.0f,
            center.y + 16.0f + 2.0f};
        DrawRectangleRounded(
            (Rectangle){textPos.x - 4, textPos.y - 4, textSz.x + 8, textSz.y + 8},
            0.3f, 8, ColorAlpha(BLACK, 0.8f));
        DrawTextEx(fontLoadingTitle, amountText.c_str(), textPos, fontSize, 0, WHITE);
    }
}

/*==============================================================================
 * ItemSpawnManager
 *==============================================================================*/

/** @brief Bobot drop berdasarkan rarity */
static const std::map<ItemRarity, int> RARITY_WEIGHTS = {
    {RARITY_COMMON, 80},
    {RARITY_UNCOMMON, 60},
    {RARITY_RARE, 40},
    {RARITY_EPIC, 20}};

/**
 * @brief Generate seed dari nama area untuk randomisasi spawn
 * @param name Nama spawn area dari Tiled
 * @return Seed hasil hash nama
 */
unsigned int ItemSpawnManager::SeedFromName(const std::string &name)
{
    unsigned int seed = 0;
    for (char c : name)
        seed = seed * 31 + static_cast<unsigned int>(c);
    return seed;
}

/**
 * @brief Inisialisasi spawn system untuk map saat ini
 *
 * Clear area lama, load dari layer Tiled, kategorisasi ukuran,
 * tentukan area aktif secara random.
 *
 * @param layerName Nama object layer di Tiled yang berisi spawn area
 */
void ItemSpawnManager::Init(const std::string &layerName)
{
    spawnAreas.clear();
    LoadSpawnAreas(layerName);
    CategorizeAreas();
    DetermineActiveAreas();
}

/**
 * @brief Baca semua spawn area dari object layer Tiled
 * @param layerName Nama layer yang dibaca
 */
void ItemSpawnManager::LoadSpawnAreas(const std::string &layerName)
{
    const auto &objects = TilesonGetObjectsByLayerName(layerName);
    if (objects.empty())
        return;

    for (auto *obj : objects)
    {
        SpawnArea area;
        area.name = obj->name;
        area.bounds = obj->bounds;
        area.isPolygon = obj->hasPolygon;
        area.isActive = false;
        spawnAreas.push_back(area);
    }
}

/**
 * @brief Kategorisasi semua spawn area berdasarkan ukuran
 *
 * Polygon selalu SMALL. Rectangle dikategorisasi berdasarkan sisi terpanjang:
 * <= 128px = SMALL, <= 256px = MEDIUM, <= 384px = LARGE, > 384px = XLARGE.
 * Tiap size class punya range minSpawn-maxSpawn sendiri.
 */
void ItemSpawnManager::CategorizeAreas()
{
    for (auto &area : spawnAreas)
    {
        if (area.isPolygon)
        {
            area.sizeClass = SPAWN_SIZE_SMALL;
            int polygonMinSpawn = 2;
            int polygonMaxSpawn = 3;
            area.minSpawn = polygonMinSpawn;
            area.maxSpawn = polygonMaxSpawn;
            continue;
        }
        area.sizeClass = ClassifySize(area.bounds.width, area.bounds.height);
        switch (area.sizeClass)
        {
        case SPAWN_SIZE_SMALL:
            area.minSpawn = SPAWN_SIZE_SMALL_MIN;
            area.maxSpawn = SPAWN_SIZE_SMALL_MAX;
            break;
        case SPAWN_SIZE_MEDIUM:
            area.minSpawn = SPAWN_SIZE_MEDIUM_MIN;
            area.maxSpawn = SPAWN_SIZE_MEDIUM_MAX;
            break;
        case SPAWN_SIZE_LARGE:
            area.minSpawn = SPAWN_SIZE_LARGE_MIN;
            area.maxSpawn = SPAWN_SIZE_LARGE_MAX;
            break;
        case SPAWN_SIZE_XLARGE:
            area.minSpawn = SPAWN_SIZE_XLARGE_MIN;
            area.maxSpawn = SPAWN_SIZE_XLARGE_MAX;
            break;
        }
    }
}

/**
 * @brief Klasifikasi ukuran area berdasarkan dimensi terpanjang
 * @param width Lebar area
 * @param height Tinggi area
 * @return SpawnAreaSize hasil klasifikasi
 */
SpawnAreaSize ItemSpawnManager::ClassifySize(float width, float height)
{
    float longest = (width > height) ? width : height;
    if (longest <= 128.0f)
        return SPAWN_SIZE_SMALL;
    if (longest <= 256.0f)
        return SPAWN_SIZE_MEDIUM;
    if (longest <= 384.0f)
        return SPAWN_SIZE_LARGE;
    return SPAWN_SIZE_XLARGE;
}

/**
 * @brief Tentukan area mana yang aktif secara random tiap run
 *
 * Jumlah area aktif di-random antara 1 sampai total area.
 * Area di-shuffle lalu sejumlah activeCount pertama ditandai aktif.
 */
void ItemSpawnManager::DetermineActiveAreas()
{
    if (spawnAreas.empty())
        return;

    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    std::uniform_int_distribution<int> areaDist(1, (int)spawnAreas.size());
    int activeCount = areaDist(rng);

    std::shuffle(spawnAreas.begin(), spawnAreas.end(), rng);
    for (int i = 0; i < activeCount; i++)
        spawnAreas[i].isActive = true;
}

/**
 * @brief Dapatkan posisi random yang aman di dalam spawn area
 *
 * Coba maksimal 100 kali dengan IsPositionSafe check.
 * Fallback ke center area jika semua attempt gagal.
 *
 * @param area SpawnArea target
 * @return Posisi valid dalam area
 */
Vector2 ItemSpawnManager::GetRandomPosInArea(const SpawnArea &area, Vector2 hitboxSize)
{
    int maxAttempts = 100;

    float halfW = hitboxSize.x / 2.0f;
    float halfH = hitboxSize.y / 2.0f;
    float minX = area.bounds.x + halfW;
    float maxX = area.bounds.x + area.bounds.width - halfW;
    float minY = area.bounds.y + halfH;
    float maxY = area.bounds.y + area.bounds.height - halfH;

    for (int i = 0; i < maxAttempts; i++)
    {
        // Generate center position dengan margin hitbox agar tidak keluar area
        Vector2 center = {
            (float)GetRandomValue((int)minX, (int)maxX),
            (float)GetRandomValue((int)minY, (int)maxY)};

        // Convert center→top-left karena IsPositionSafe expect top-left
        Vector2 topLeft = {center.x - halfW, center.y - halfH};
        if (IsPositionSafe(topLeft, hitboxSize.x, hitboxSize.y, 0, 0))
            return center;
    }

    // fallback ke center area
    return {area.bounds.x + area.bounds.width / 2,
            area.bounds.y + area.bounds.height / 2};
}

// Pilih definitionId random berdasarkan rarity weight
int ItemSpawnManager::PickRandomDefinitionId(std::mt19937 &rng, ItemCategory filterCategory)
{
    // Kumpulkan semua item per rarity
    std::map<ItemRarity, std::vector<int>> byRarity;
    for (const auto &[name, def] : itemDefs.GetAll())
    {
        if (filterCategory != ITEM_ANY && def.category != filterCategory)
            continue;
        byRarity[def.rarity].push_back(def.id);
    }

    // Hitung total dulu
    int total = 0;
    for (const auto &[rarity, weight] : RARITY_WEIGHTS)
        total += weight;

    // Roll rarity dulu
    std::uniform_int_distribution<int> rollDist(1, total);
    int roll = rollDist(rng);

    int cumulative = 0;
    ItemRarity pickedRarity = RARITY_COMMON;
    for (const auto &[rarity, weight] : RARITY_WEIGHTS)
    {
        cumulative += weight;
        if (roll <= cumulative)
        {
            pickedRarity = rarity;
            break;
        }
    }

    // Kalau rarity yang ke-roll gak ada itemnya, fallback ke COMMON
    if (byRarity[pickedRarity].empty())
        pickedRarity = RARITY_COMMON;

    if (byRarity[pickedRarity].empty())
        return -1; // gak ada item yang cocok

    // Pilih random dari item dengan rarity itu
    std::uniform_int_distribution<int> idxDist(0, (int)byRarity[pickedRarity].size() - 1);
    return byRarity[pickedRarity][idxDist(rng)];
}

/**
 * @brief Spawn semua item ke activeItems berdasarkan area aktif
 *
 * Tiap area aktif spawn item sejumlah random antara minSpawn-maxSpawn.
 * Category dan rarity di-random per item.
 * Log total item yang di-spawn di akhir.
 *
 * @param activeItems Referensi ke vector item yang akan diisi
 */
void ItemSpawnManager::SpawnAll(std::vector<ItemSpawn> &activeItems)
{
    activeItems.clear();

    for (auto &area : spawnAreas)
    {
        if (!area.isActive)
            continue;

        unsigned int globalSeed = static_cast<unsigned int>(time(nullptr));
        unsigned int nameSeed = SeedFromName(area.name);
        std::mt19937 rng(globalSeed ^ nameSeed);
        std::uniform_int_distribution<int> countDist(area.minSpawn, area.maxSpawn);
        int spawnCount = countDist(rng);

        for (int i = 0; i < spawnCount; i++)
        {
            int defId = PickRandomDefinitionId(rng);
            const ItemDefinition &def = itemDefs.GetById(defId);
            Vector2 pos = GetRandomPosInArea(area, def.hitboxSize);
            activeItems.push_back(itemData.CreateItem(pos, defId));
        }
    }

    TraceLog(LOG_INFO, "ITEM: total spawned = %d", (int)activeItems.size());
}
