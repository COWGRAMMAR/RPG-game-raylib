#pragma once

/**
 * @file map.h
 * @brief Map System & Tileson Integration Module
 *
 * Header ini mendeklarasikan data dan fungsi utama untuk:
 * - Load dan unload map dari Tiled JSON
 * - Render tile map
 * - Simpan object map dan tileset
 * - Hitung visible tile range untuk culling
 */

#include "../lib/raylib/include/raylib.h"
#include "../lib/tileson/tileson.hpp"
#include "screen.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include "mapstack.h"

/*==============================================================================
 * Global Camera & Map History Stack
 *==============================================================================*/

/** @brief Camera global untuk rendering world */
extern Camera2D camera;

// Stack riwayat perpindahan map (global agar bisa diakses oleh save system)
extern MapSystem::MapStack mapHistoryStack;

/*==============================================================================
 * MapObject Struct
 *==============================================================================*/

/**
 * @brief Representasi satu object dari object layer Tiled
 */
struct MapObject
{
    int id;                                           // ID unik dari Tiled
    std::string name;                                 // Nama object di Tiled
    std::string type;                                 // Type object di Tiled
    std::string layerName;                            // Nama layer asal object
    Rectangle bounds;                                 // Bounding box object
    std::vector<Vector2> polygonPoints;               // Titik polygon dalam world space
    bool hasPolygon = false;                          // True jika object punya polygon
    std::map<std::string, tson::Property> properties; // Custom properties dari object
};

/*==============================================================================
 * TilesetInfo Struct
 *==============================================================================*/

/**
 * @brief Menyimpan data tileset yang dibutuhkan saat rendering
 */
struct TilesetInfo
{
    Texture2D texture; // Texture tileset yang sudah dimuat
    int cols;          // Jumlah kolom tile dalam tileset
    int spacing;       // Jarak antar tile dalam tileset
    int firstgid;      // Global ID pertama dari tileset
    int lastgid;       // Global ID terakhir dari tileset
};

/*==============================================================================
 * TilesonMapData Struct
 *==============================================================================*/

/**
 * @brief Precomputed index untuk O(1) lookup MapObject
 *
 * Dibangun sekali via BuildMapObjectIndex() setelah LoadMap().
 * Semua pointer nunjuk langsung ke element di tilesonMap->Objects — tidak ada copy.
 */
struct MapObjectIndex
{
    std::unordered_map<std::string, MapObject *> byName;
    std::unordered_map<std::string, std::vector<MapObject *>> byType;
    std::unordered_map<std::string, std::vector<MapObject *>> byLayer;
};

/**
 * @brief Menyimpan seluruh data map hasil parse Tiled
 */
struct TilesonMapData
{
    int width = 0;                                  // Lebar map dalam satuan tile
    int height = 0;                                 // Tinggi map dalam satuan tile
    int layerCount = 0;                             // Jumlah tile layer
    int **tiles = nullptr;                          // Data tile untuk tiap layer
    std::vector<std::vector<TilesetInfo>> tilesets; // Daftar tileset yang dipakai map
    std::vector<int> layerTilesetGroup;             // layer index → group index
    std::vector<MapObject> Objects;                 // Daftar object dari object layer
    MapObjectIndex objectIndex;                     // Index object Tiled yang dipakai untuk lookup cepat
};

/** @brief Pointer ke map aktif */
extern TilesonMapData *tilesonMap;

/*==============================================================================
 * TileRange Struct
 *==============================================================================*/

/**
 * @brief Range tile yang terlihat di layar
 */
struct TileRange
{
    int minX; // Kolom minimum yang terlihat
    int minY; // Baris minimum yang terlihat
    int maxX; // Kolom maksimum yang terlihat (exclusive)
    int maxY; // Baris maksimum yang terlihat (exclusive)
};

/**
 * @brief Hitung range tile yang terlihat berdasarkan camera
 * @return TileRange untuk kebutuhan frustum culling
 */
TileRange GetVisibleTileRange(void);

/** @brief Dapatkan visible world rect dari camera */
Rectangle GetVisibleWorldRect(void);

/*==============================================================================
 * Debug Variables
 *==============================================================================*/

/** @brief Jumlah tile render frame terakhir */
extern int lastTilesRendered;

/** @brief Range tile visible frame terakhir */
extern TileRange currentVisibleRange;

/*==============================================================================
 * Tiled Layer & Object Name Constants
 *==============================================================================*/

constexpr const char *COLLISION_LAYER_NAME = "obstacle"; // Nama layer collision obstacle
constexpr const char *OBJECT_LAYER_NAME = "object";      // Nama layer object placement
constexpr const char *TRAP_LAYER_NAME = "trap";          // nama layer trap placement
constexpr const char *ITEM_LAYER_NAME = "item";          // nama layer item placment
constexpr const char *EXIT_LAYER_NAME = "exit";          // nama layer exit placement
/** @brief Nama objek spesifik untuk spawn musuh */
constexpr const char *ENEMY_SPAWN_NORMAL_PIN_OBJECT_NAME = "enemy_spawn_normal_pinpoint"; // Nama object spawn normal pinpoint
constexpr const char *ENEMY_SPAWN_NORMAL_REC_OBJECT_NAME = "enemy_spawn_normal_rect";     // Nama object rect spawn normal
constexpr const char *ENEMY_SPAWN_ELITE_PIN_OBJECT_NAME = "enemy_spawn_elite_pinpoint";   // Nama object pinpoint spawn elite
constexpr const char *ENEMY_SPAWN_ELITE_REC_OBJECT_NAME = "enemy_spawn_elite_rect";       // Nama object rect spawn elite
constexpr const char *ENEMY_SPAWN_BOSS_OBJECT_NAME = "enemy_spawn_boss";                  // Nama object spawn boss
constexpr const char *SPAWN_OBJECT_NAME = "spawn";                                        // Nama object spawn player
constexpr const char *DOOR_TYPE_OBJECT_NAME = "pass";                                     // Type object untuk pintu
constexpr const char *BARRIER_TYPE_OBJECT_NAME = "barrier";                               // Type object untuk barrier
constexpr const char *BARRIER_BOSS_TYPE_OBJECT_NAME = "barrier_boss";                     // Type object untuk barrier boss
constexpr const char *CHEST_TYPE_OBJECT_NAME = "chest";                                   // Type object untuk chest
constexpr const char *SPIKE_TYPE_OBJECT_NAME = "spike";                                   // Type object unutk spike
constexpr const char *BOMB_TYPE_OBJECT_NAME = "bomb";                                     // type object untuk bomb
constexpr const char *CRATE_TYPE_OBJECT_NAME = "crate";                                   // type object untuk crate
constexpr const char *SIGN_TYPE_OBJECT_NAME = "sign";                                     // Type object untuk sign
constexpr const char *BOSS_STAGE_TYPE_OBJECT_NAME = "boss_stage";                         // Type object untuk trigger area boss
/*==============================================================================
 * Map Functions
 *==============================================================================*/

/**
 * @brief Load map dari file JSON Tiled
 * @param mapPath Path file map yang akan dimuat
 */
void LoadMap(const char *mapPath);

/** @brief Render map ke layar */
void RenderMap(void);

/** @brief Bersihkan seluruh data map yang sedang aktif */
void UnloadMap(void);

/** @brief Inisialisasi map awal game */
void InitMap(void);

/**
 * @brief Pindah ke map baru di titik tujuan tertentu
 * @param newMapPath Path file map tujuan
 * @param targetSpawnName Nama spawn point atau pintu tujuan
 */
void SwitchMap(const char *newMapPath, const char *targetSpawnName);

/**
 * @brief Dapatkan path map yang sedang aktif
 * @return Path map saat ini
 */
const char *GetCurrentMapPath(void);

/**
 * @brief Set path map yang sedang aktif
 * @param newPath Path map baru
 */
void SetCurrentMapPath(const char *newPath);

/** @brief Kembali ke map sebelumnya */
void GoBack(void);

/**
 * @brief Generate world di map yang sudah di-load
 * @param seed Seed deterministic
 */
void RunWorldgen(uint64_t seed, bool isBossStage = false);

/**
 * @brief Trim stack riwayat map — sisakan cuma 1 entry teratas (immediate prev stage)
 *
 * Pas stage transition, map lama di-push ke stack. Tapi kita cuman mau
 * nyimpen 1 stage sebelumnya (bukan semua riwayat). Sisanya di-pop.
 */
void TrimStageStack(void);

/**
 * @brief Dapatkan nama tampilan map dari file path.
 * @param mapFilePath Path file map (contoh: "assets/maps/town.json")
 * @return Nama map yang mudah dibaca (contoh: "Town")
 * @note Konversi path-based: ekstrak nama file tanpa ekstensi, capitalisasi,
 *       handle special case untuk worldgen stages.
 */
std::string GetMapDisplayName(const std::string& mapFilePath);
