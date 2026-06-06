#pragma once

/**
 * @file item.h
 * @brief Item System Module
 *
 * Header ini mendeklarasikan data dan fungsi utama untuk:
 * - Menyimpan data item dan spawn area
 * - Spawn item dari object layer Tiled
 * - Render item di world
 * - Update pickup dan magnet item
 * - Save/load state item per map
 */

#include "../lib/raylib/include/raylib.h"
#include <vector>
#include <string>

class Player; // Forward declaration untuk DropAllItems
#include <map>
#include <random>
#include <algorithm>
#include <variant>
#include <unordered_map>

/*==============================================================================
 * Enums
 *==============================================================================*/

/**
 * @brief Kategori utama item
 */
typedef enum
{
    ITEM_WEAPON, // Item senjata (di json nanti namanya harus ditulis weapon)
    ITEM_POTION, // Item potion/healing (di json nanti namanya harus ditulis potion)
    ITEM_POISON, // Item poison (di json nanti namanya harus ditulis posion)
    ITEM_ARMOR,  // Item armor (di json nanti namanya harus ditulis armor)
    ITEM_NONE,   // Tidak ada item
    ITEM_ANY     // Tidak ada filter kategori
} ItemCategory;

/**
 * @brief Tingkat rarity item
 */
typedef enum
{
    RARITY_COMMON,   // Item biasa (di json nanti namanya harus ditulis common)
    RARITY_UNCOMMON, // item gak biasa (di json nanti namanya harus ditulis uncommon)
    RARITY_RARE,     // Item langka (di json nanti namanya harus ditulis rare)
    RARITY_EPIC      // item epik (di json nanti namanya harus ditulis epic)
} ItemRarity;

/**
 * @brief Klasifikasi ukuran area spawn item
 */
typedef enum
{
    SPAWN_SIZE_SMALL,  // Area spawn kecil
    SPAWN_SIZE_MEDIUM, // Area spawn sedang
    SPAWN_SIZE_LARGE,  // Area spawn besar
    SPAWN_SIZE_XLARGE  // Area spawn sangat besar
} SpawnAreaSize;

/*==============================================================================
 * Behavior Structs per Category
 *==============================================================================*/

/** @brief Tipe serangan */
enum AttackType
{
    ATTACK_SLASH,  // (di json nanti namanya harus ditulis slash)
    ATTACK_THRUST, // (di json nanti namanya harus ditulis thrust)
    ATTACK_PIERCE, // (di json nanti namanya harus ditulis pierce)
    ATTACK_SLAM    // (di json nanti namanya harus ditulis slam)
};

/**
 * @brief Data spesifik untuk item bertipe senjata.
 */
struct WeaponData
{
    float damage;           // Damage yang diberikan ke musuh per hit
    float reach;            // Jangkauan serangan (panjang hitbox)
    float breadth;          // Lebar hitbox serangan
    float duration;         // Durasi animasi serangan (detik)
    float knockbackForce;   // Kekuatan knockback ke musuh
    float startAngleOffset; // Offset sudut awal serangan (derajat)
    float sweepAngle;       // Total sudut sapuan serangan (derajat)
    Vector2 centerOffset;   // Offset pusat hitbox relatif ke posisi player
    float manaCost;         // Mana yang dikonsumsi saat menyerang
    AttackType attackType;  // Jenis serangan
};

/**
 * @brief Data spesifik untuk item bertipe potion.
 */
struct PotionData
{
    int healValue; // Jumlah HP atau mana yang dipulihkan
    bool isMana;   // True jika potion memulihkan mana, bukan HP
};

/**
 * @brief Data spesifik untuk item bertipe armor.
 * @note Belum diimplementasikan sepenuhnya.
 */
struct ArmorData
{
    float defense; // Nilai pertahanan yang ditambahkan ke player
};

/*==============================================================================
 * ItemDefinition — single source of truth
 *==============================================================================*/

/**
 * @brief Definisi statis sebuah item, dimuat dari JSON saat startup.
 * @note Tidak boleh dimodifikasi setelah Load() selesai.
 */
struct ItemDefinition
{
    int id;                                               // ID numerik unik, dipakai untuk lookup di runtime
    std::string name;                                     // Nama item, sekaligus key di unordered_map
    std::string spriteKey;                                // Key visual untuk merender item (di-load dari JSON)
    ItemCategory category;                                // Kategori item (weapon, potion, armor, dll)
    Vector2 sheetCoord;                                   // Koordinat tile di spritesheet
    Vector2 hitboxSize;                                   // Ukuran hitbox item saat di-spawn di dunia
    ItemRarity rarity;                                    // Rarity item, dipakai untuk drop rate dan visual
    bool isStackable;                                     // True jika item bisa ditumpuk di inventory
    int maxStack;                                         // Batas maksimum tumpukan per slot inventory
    std::variant<WeaponData, PotionData, ArmorData> data; // Data spesifik berdasarkan category
};

/*==============================================================================
 * Structs
 *==============================================================================*/

/**
 * @brief Representasi satu instance item yang sudah di-spawn di dunia.
 */
struct ItemSpawn
{
    int definitionId; // Merujuk ke ItemDefinition::id
    Vector2 position; // Posisi item di world space
    Rectangle hitbox; // Hitbox untuk deteksi pickup oleh player
    bool isPickedUp;  // True jika item sudah diambil player
    bool isAdded;     // True jika item sudah ditambahkan ke inventory
    float spawnTime;  // Timestamp saat item di-spawn (untuk efek atau despawn)
    int amount = 1;   ///< Jumlah item dalam satu spawn (untuk stackable item)
    std::string uuid; ///< Unique identifier for persistent entity matching across save/load cycles. Generated at spawn time, persisted in save files, used for restore matching.
};

/**
 * @brief Satu slot di inventory player.
 */
struct InventoryItem
{
    int definitionId = -1; // ID item yang ada di slot ini; -1 = slot kosong
    int amount = 0;        // Jumlah item di slot ini
};

/**
 * @brief Representasi satu area spawn item dari Tiled.
 */
typedef struct
{
    std::string name;        // Nama object layer dari Tiled, dipakai untuk identifikasi
    Rectangle bounds;        // Bounding box area spawn di world space
    SpawnAreaSize sizeClass; // Kategori ukuran area (small, medium, large)
    int minSpawn;            // Jumlah minimum item yang di-spawn di area ini
    int maxSpawn;            // Jumlah maksimum item yang di-spawn di area ini
    bool isActive;           // True jika area ini dipilih untuk run saat ini
    bool isPolygon;          // True jika area berasal dari polygon Tiled, bukan rectangle
} SpawnArea;

/*==============================================================================
 * ItemDefinitionManager
 *==============================================================================*/

/**
 * @brief Mengelola semua definisi item yang dimuat dari JSON.
 *
 * Diisi satu kali saat startup via Load(), lalu hanya dibaca.
 * Semua lookup item di runtime dilakukan lewat class ini.
 */
class ItemDefinitionManager
{
public:
    /**
     * @brief Memuat semua definisi item dari file JSON.
     * @param path Path ke file JSON (contoh: "assets/data/items.json")
     * @throws std::runtime_error Jika file tidak bisa dibuka
     */
    void Load(const std::string &path);

    /**
     * @brief Mencari definisi item berdasarkan ID numerik.
     * @param id ID item yang dicari
     * @return Referensi ke ItemDefinition yang cocok
     * @throws std::runtime_error Jika ID tidak ditemukan
     * @note Linear search — acceptable untuk pool item kecil
     */
    const ItemDefinition &GetById(int id) const;

    /** @brief Ambil semua definisi item */
    const std::unordered_map<std::string, ItemDefinition> &GetAll() const;

    /**
     * @brief Dapatkan hitboxSize terbesar dari semua item dalam kategori tertentu
     * @param category Kategori item yang dicek (ITEM_ANY = semua)
     * @return Vector2 hitboxSize maksimum
     */
    Vector2 GetMaxHitboxForCategory(ItemCategory category) const;

private:
    std::unordered_map<std::string, ItemDefinition> definitions_; // Key = nama item
    std::unordered_map<int, const ItemDefinition *> byId_;        // Key = ID numerik, untuk O(1) lookup
};

/*==============================================================================
 * ItemDataManager
 *==============================================================================*/

/**
 * @brief Mengelola data item aktif, spawn langsung, dan save/load per map
 */
class ItemDataManager
{
public:
    /**
     * @brief Inisialisasi data item
     */
    void Init();

    /**
     * @brief Buat item baru berdasarkan definisi
     * @param pos Posisi spawn item
     * @param definitionId ID definisi item
     * @return ItemSpawn yang sudah dibuat
     */
    ItemSpawn CreateItem(Vector2 pos, int definitionId);
    /**
     * @brief Spawn item langsung di posisi tertentu
     * @param pos Posisi spawn item
     */
    void SpawnItemAtLocation(Vector2 pos, std::mt19937 *rng = nullptr, ItemCategory category = ITEM_ANY);

    /**
     * @brief Simpan state item aktif untuk map tertentu
     * @param mapPath Path map sebagai key penyimpanan
     */
    void SaveItemsForMap(const std::string &mapPath);

    /**
     * @brief Load state item aktif untuk map tertentu
     * @param mapPath Path map sebagai key pencarian
     * @return true jika data map ditemukan, false jika belum ada
     */
    bool LoadItemsForMap(const std::string &mapPath);

    /**
     * @brief Hapus semua item aktif
     */
    void ClearItems();

    std::vector<ItemSpawn> activeItems; // Daftar item yang sedang aktif di world

private:
    // Penyimpanan state item berdasarkan path map
    // @deprecated Use SaveItemsForMapDir / LoadItemsForMapDir free functions instead
    std::map<std::string, std::vector<ItemSpawn>> savedMapItems;
    std::unordered_map<std::string, ItemDefinition> definitions_; // Definisi item lokal
};

/*==============================================================================
 * ItemRenderManager
 *==============================================================================*/

/**
 * @brief Mengelola update dan render item di world
 */
class ItemRenderManager
{
public:
    /**
     * @brief Update magnet item dan deteksi pickup
     * @param items Daftar item yang akan di-update
     * @param playerCenter Posisi tengah player
     * @param playerHitbox Hitbox player untuk deteksi pickup
     * @param magnetRadius Radius tarikan item ke player
     * @param itemSpeed Kecepatan item saat tertarik ke player
     */
    void Update(std::vector<ItemSpawn> &items, Vector2 playerCenter,
                Rectangle playerHitbox, float magnetRadius, float itemSpeed);

    /**
     * @brief Render semua item yang belum diambil
     * @param items Daftar item yang akan dirender
     */
    int RenderAll(std::vector<ItemSpawn> &items, Rectangle viewRect);

    /** @brief Render satu item */
    void Render(ItemSpawn &item);

private:
};

/*==============================================================================
 * ItemSpawnManager
 *==============================================================================*/

/**
 * @brief Mengelola spawn area dari Tiled dan distribusi item
 */
class ItemSpawnManager
{
public:
    /**
     * @brief Inisialisasi spawn manager dari layer Tiled tertentu
     * @param layerName Nama object layer yang berisi area spawn item
     */
    void Init(const std::string &layerName);

    /**
     * @brief Spawn semua item berdasarkan area aktif
     * @param activeItems Vector item aktif yang akan diisi
     */
    void SpawnAll(std::vector<ItemSpawn> &activeItems);

    /**
     * @brief Pilih random definition ID dari pool item
     * @param rng Reference ke RNG
     * @param filterCategory Filter kategori (default: semua)
     * @return ID definisi item yang terpilih
     */
    int PickRandomDefinitionId(std::mt19937 &rng, ItemCategory filterCategory = ITEM_ANY);

private:
    // Daftar area spawn item yang dibaca dari Tiled
    std::vector<SpawnArea> spawnAreas;

    void LoadSpawnAreas(const std::string &layerName);                     // Load spawn area dari object layer Tiled
    void CategorizeAreas();                                                // Kategorisasi area berdasarkan ukuran
    SpawnAreaSize ClassifySize(float width, float height);                 // Klasifikasikan ukuran area spawn
    void DetermineActiveAreas();                                           // Tentukan area spawn yang aktif
    Vector2 GetRandomPosInArea(const SpawnArea &area, Vector2 hitboxSize); // Ambil posisi random di area
    unsigned int SeedFromName(const std::string &name);                    // Buat seed random dari nama area

    int SPAWN_SIZE_SMALL_MIN = 1;  // jumlah minimum spawn untuk area kecil
    int SPAWN_SIZE_SMALL_MAX = 2;  // jumlah maksimum spawn untuk area kecil
    int SPAWN_SIZE_MEDIUM_MIN = 2; // jumlah minimum spawn untuk area sedang
    int SPAWN_SIZE_MEDIUM_MAX = 3; // jumlah maksimum spawn untuk area sedang
    int SPAWN_SIZE_LARGE_MIN = 3;  // jumlah minimum spawn untuk area besar
    int SPAWN_SIZE_LARGE_MAX = 4;  // jumlah maksimum spawn untuk area besar
    int SPAWN_SIZE_XLARGE_MIN = 4; // jumlah minimum spawn untuk area sangat besar
    int SPAWN_SIZE_XLARGE_MAX = 5; // jumlah maksimum spawn untuk area sangat besar
};

/*==============================================================================
 * Free functions (backward compat, wrapper ke class di atas)
 *==============================================================================*/

/**
 * @brief Inisialisasi seluruh sistem item
 */
void InitItems();

/**
 * @brief Reset dan spawn ulang item untuk map/run saat ini
 */
void SpawnItemWave();

/**
 * @brief Spawn item random melalui spawn manager
 */
void SpawnRandomItem();

/** @brief Ambil daftar item spawn aktif */
std::vector<ItemSpawn> &GetActiveItems();

/** @brief Drop semua item player ke ground saat mati */
void DropAllItems(Player &player);

/** @brief Instance global ItemDataManager */
extern ItemDataManager itemData;
/** @brief Instance global ItemRenderManager */
extern ItemRenderManager itemRender;
/** @brief Instance global ItemSpawnManager */
extern ItemSpawnManager spawnManager;
/** @brief Instance global ItemDefinitionManager */
extern ItemDefinitionManager itemDefs;

/*==============================================================================
 * Per-Map File Persistence (replaces in-memory ItemDataManager::savedMapItems)
 *==============================================================================*/

/**
 * @brief Save all active items for a map to the specified directory.
 *
 * Serializes `itemData.activeItems` to a JSON file at `<baseDir>/<sanitized_path>`.
 * Uses atomic write via .tmp file + rename to prevent corruption.
 * Follows the same pattern as SaveEnemiesForMap() in enemy.cpp.
 *
 * @param mapPath Raw map file path used to derive the save file name (e.g., "assets/maps/tutorial.json")
 * @param baseDir Base directory for save files (default: "saves/items")
 */
void SaveItemsForMapDir(const std::string &mapPath, const std::string &baseDir = "saves/items");

/**
 * @brief Load items for a map from the specified directory.
 *
 * Reads `<baseDir>/<sanitized_path>`, deserializes each item's fields,
 * reconstructs hitboxes from definitions, and populates `itemData.activeItems`.
 *
 * @param mapPath Raw map file path used to derive the save file name
 * @param baseDir Base directory for save files (default: "saves/items")
 * @return true if items were loaded, false if no save data exists or parse failed
 */
bool LoadItemsForMapDir(const std::string &mapPath, const std::string &baseDir = "saves/items");
