#pragma once
#include "map.h"
#include "animation.h"
#include <vector>
#include <string>
#include <random>

/**
 * @file worldgenenartion.h
 * @brief World Generation Core Module
 *
 * Header ini mendeklarasikan data, struct, enum, utility functions, dan tiga class utama
 * untuk sistem world generation:
 * - WorldGenPools: manajemen pool prefab room dan corridor
 * - WorldGenPrefab: load/unload/rotate satu prefab
 * - WorldGenCanvas: stamping prefab dan corridor ke main map
 */

/*==============================================================================
 * Constants
 *==============================================================================*/

constexpr int WG_GRID_SIZE = 4;            // Jumlah cell per baris/kolom grid worldgen
constexpr int WG_CELL_TILES = 41;          // Ukuran satu cell dalam satuan tile
constexpr int WG_CANVAS_TILES = 164;       // Total tile per sisi canvas (GRID_SIZE * CELL_TILES)
constexpr int WG_PREFAB_LAYER_START = 3;   // Layer awal untuk nge-stamp prefab room
constexpr int WG_CORRIDOR_LAYER_START = 5; // Layer awal untuk nge-stamp corridor
constexpr int WG_TILE_SIZE = FRAME_SIZE;   // Ukuran tile dalam pixel

// Prim's Algorithm — tuning jumlah cell dan stop chance
constexpr int WG_PRIM_START_CELLS = 1;   // Jumlah cell awal saat seed Prim
constexpr int WG_PRIM_MIN_CELLS = 6;     // Minimum cell sebelum boleh random stop
constexpr int WG_PRIM_MAX_CELLS = 10;    // Maksimum cell (batas atas loop Prim)
constexpr int WG_PRIM_STOP_CHANCE = 20;  // Persentase chance random stop setelah MIN_CELLS
constexpr int WG_PRIM_RETRY_MAX = 10;    // Maksimum percobaan generate layout
constexpr int WG_VARIETY_ITERATIONS = 2; // Iterasi variety pruning per generate

constexpr int NUM_DIRS = 4;         // Jumlah arah mata angin
constexpr int DEPTH_UNVISITED = -1; // Sentinel: cell belum dikunjungi di ComputeDepth
constexpr int INVALID_INDEX = -1;   // Sentinel: index / koordinat tidak valid
constexpr int PERCENT_MAX = 100;    // Nilai maksimum persentase untuk RNG

constexpr int CORRIDOR_CENTER_OFFSET = 3; // Offset tile untuk center corridor saat stamp
constexpr int CORRIDOR_LAYER_COUNT = 2;   // Jumlah layer corridor (vertical + horizontal)

constexpr const char *SLOT_WORLDGEN_LAYER_NAME = "slot_worldgen";
constexpr const char *EXIT_NORTH_TYPE_OBJECT_NAME = "exit_north";
constexpr const char *EXIT_EAST_TYPE_OBJECT_NAME = "exit_east";
constexpr const char *EXIT_SOUTH_TYPE_OBJECT_NAME = "exit_south";
constexpr const char *EXIT_WEST_TYPE_OBJECT_NAME = "exit_west";

/*==============================================================================
 * Enums
 *==============================================================================*/

/**
 * @brief Arah exit yang tersedia pada suatu room
 *
 * Bitmask: bisa di-combine untuk room dengan multiple exit.
 */
enum ExitDirection
{
    EXIT_NONE = 0,
    EXIT_NORTH = 1 << 0,
    EXIT_EAST = 1 << 1,
    EXIT_SOUTH = 1 << 2,
    EXIT_WEST = 1 << 3,
};

/**
 * @brief Tipe cell dalam grid world generation
 */
enum CellType
{
    CELL_EMPTY = 0,       // Cell kosong / tidak dipakai
    CELL_START = 1,       // Starting room pemain
    CELL_ENEMY = 2,       // Room musuh biasa
    CELL_ENEMY_ELITE = 3, // Room musuh elite
    CELL_TREASURE = 4,    // Room harta karun
    CELL_TRADER = 5,      // Room trader
    CELL_FINISH = 6,      // Room finish
    CELL_BOSS = 7,        // Room boss
    CELL_SPECIAL = 8,     // Room special event
};

/*==============================================================================
 * Structs
 *==============================================================================*/

/**
 * @brief Satu cell dalam grid world generation
 */
struct WorldCell
{
    CellType type; // Tipe cell
    int exitMask;  // Bitmask exit yang aktif
};

/**
 * @brief Pool berisi kumpulan prefab dengan flag loaded
 */
struct WeightedPool
{
    std::vector<TilesonMapData *> prefabs; // Kumpulan prefab yang sudah di-load
    bool loaded = false;                   // true jika pool sudah di-load
};

/**
 * @brief Pool corridor — vertical dan horizontal
 */
struct CorridorPool
{
    WeightedPool vertical;   // Pool corridor arah vertical
    WeightedPool horizontal; // Pool corridor arah horizontal
    bool loaded = false;     // true jika corridor pool sudah di-load
};

/**
 * @brief Pool room berdasarkan konfigurasi exit
 *
 * Masing-masing pool berisi room dengan jumlah/jenis exit tertentu:
 * u = 1 exit (bisa utara/selatan/timur/barat)
 * ud = 2 exit (utara + selatan)
 * ur = 2 exit (utara + timur) — L-shape
 * udr = 3 exit (utara + selatan + timur)
 * udrl = 4 exit (semua arah)
 */
struct RoomPool
{
    WeightedPool u;      // 1 exit
    WeightedPool ud;     // 2 exit lurus (utara-selatan)
    WeightedPool ur;     // 2 exit L-shape (utara-timur)
    WeightedPool udr;    // 3 exit (utara-selatan-timur)
    WeightedPool udrl;   // 4 exit
    bool loaded = false; // true jika room pool sudah di-load
};

/*==============================================================================
 * GridMath — Utility Functions
 *==============================================================================*/

/**
 * @brief Utility functions untuk manipulasi grid 2D dan rotasi
 */
namespace GridMath
{
    /**
     * @brief Ubah data tile flat 1D ke grid 2D
     * @param flatData Data tile dalam bentuk vector 1D
     * @param width Lebar grid
     * @param height Tinggi grid
     * @return Grid 2D dengan ukuran height x width
     */
    std::vector<std::vector<int>> ReshapeTo2D(const std::vector<int> &flatData, int width, int height);

    /**
     * @brief Ubah grid 2D kembali ke data tile flat 1D
     * @param grid Grid 2D input
     * @return Vector 1D hasil flatten
     */
    std::vector<int> FlattenTo1D(const std::vector<std::vector<int>> &grid);

    /**
     * @brief Rotasi grid 90 derajat searah jarum jam
     * @param grid Grid 2D input
     * @return Grid hasil rotasi (ukuran width/height tertukar)
     */
    std::vector<std::vector<int>> Rotate90CW(const std::vector<std::vector<int>> &grid);

    /**
     * @brief Rotasi grid dengan kelipatan 90 derajat
     * @param grid Grid 2D input
     * @param degrees Sudut rotasi (0, 90, 180, 270)
     * @return Grid hasil rotasi
     */
    std::vector<std::vector<int>> RotateGrid(const std::vector<std::vector<int>> &grid, int degrees);

    /**
     * @brief Rotasi titik 2D terhadap center dengan kelipatan 90 derajat
     * @param point Titik yang akan dirotasi
     * @param center Pusat rotasi
     * @param degrees Sudut rotasi (0, 90, 180, 270)
     * @return Titik hasil rotasi
     */
    Vector2 RotatePoint(Vector2 point, Vector2 center, int degrees);
}

/*==============================================================================
 * Class: WorldGenPools
 *==============================================================================*/

/**
 * @brief Manajemen pool prefab room dan corridor
 *
 * Bertanggung jawab untuk load/unload seluruh prefab dari folder asset,
 * menyimpannya dalam pool, dan menyediakan akses deterministic
 * ke prefab corridor berdasarkan seed + posisi.
 */
class WorldGenPools
{
public:
    /**
     * @brief Inisialisasi pool dengan seed tertentu
     * @param seed Seed untuk deterministic corridor selection
     */
    WorldGenPools(uint64_t seed);

    /**
     * @brief Load semua room template dari folder
     * @param basePath Base path ke folder room templates
     */
    void LoadRoomPool(const char *basePath);

    /** @brief Unload semua room template dan kosongkan pool */
    void UnloadRoomPool();

    /**
     * @brief Load semua corridor prefab dari folder
     * @param basePath Base path ke folder corridor prefabs
     */
    void LoadCorridorPool(const char *basePath);

    /** @brief Unload semua corridor prefab dan kosongkan pool */
    void UnloadCorridorPool();

    /**
     * @brief Ambil corridor prefab secara deterministic dari pool
     * @param pool Pool corridor yang akan dipilih (vertical/horizontal)
     * @param tileX Posisi X dalam tile untuk deterministic hash
     * @param tileY Posisi Y dalam tile untuk deterministic hash
     * @param exitTypeHash Hash tipe exit (1-4) untuk salt
     * @return Pointer ke prefab corridor yang terpilih
     */
    TilesonMapData *GetRandomCorridor(WeightedPool &pool, int tileX, int tileY, int exitTypeHash);

    /** @return Reference ke internal CorridorPool */
    CorridorPool &GetCorridorPool();

    /**
     * @brief Ambil RoomPool untuk CellType tertentu
     * @param type Tipe cell yang pool-nya diminta
     * @return Reference ke RoomPool yang sesuai dengan type
     */
    RoomPool &GetPoolForType(CellType type);

    /**
     * @brief Kumpulkan semua prefab dari seluruh type pool
     * @return Vector berisi pointer ke semua prefab room
     */
    std::vector<TilesonMapData *> GetAllRoomPrefabs();

private:
    uint64_t worldSeed;        // Seed global untuk deterministic generation
    RoomPool enemyPool;        // Pool room tipe enemy
    RoomPool treasurePool;     // Pool room tipe treasure
    RoomPool elitePool;        // Pool room tipe elite
    RoomPool traderPool;       // Pool room tipe trader
    RoomPool startPool;        // Pool room tipe start
    RoomPool finishPool;       // Pool room tipe finish
    RoomPool bossPool;         // Pool room tipe boss
    CorridorPool corridorPool; // Pool corridor prefabs

    /**
     * @brief SplitMix64 hash function untuk deterministic selection
     * @param x Input 64-bit
     * @return Output hash 64-bit
     */
    uint64_t SplitMix64(uint64_t x);

    /**
     * @brief Pilih index corridor secara deterministic
     * @param tileX Posisi X tile
     * @param tileY Posisi Y tile
     * @param exitTypeHash Hash tipe exit (1-4)
     * @param poolSize Ukuran pool
     * @return Index terpilih dalam range [0, poolSize)
     */
    int PickCorridorIndex(int tileX, int tileY, int exitTypeHash, int poolSize);

    void LoadRoomPoolForType(const char *basePath, const char *roomType, RoomPool &targetPool); // Load room pool untuk satu tipe room
};

/*==============================================================================
 * Class: WorldGenPrefab
 *==============================================================================*/

/**
 * @brief Representasi satu prefab room/corridor yang bisa di-load, dirotate, dan di-unload
 *
 * WorldGenPrefab mengelola siklus hidup TilesonMapData untuk satu prefab.
 * Unload harus dipanggil manual — tidak ada RAII destructor.
 */
class WorldGenPrefab
{
public:
    /** @brief Constructor — data = nullptr */
    WorldGenPrefab();

    /**
     * @brief Constructor — wrap existing TilesonMapData* (non-owning)
     * @param existingData Pointer ke data yang sudah di-load (milik pool)
     *
     * Untuk bungkus prefab dari pool sementara, biarkan Rotate() yang bikin owned copy.
     * Jangan panggil Unload() pada wrapper ini — data tetap milik pool.
     */
    WorldGenPrefab(TilesonMapData *existingData);

    /**
     * @brief Load prefab dari file JSON Tiled
     * @param mapPath Path ke file JSON
     */
    void Load(const char *mapPath);

    /** @brief Unload prefab dan bebaskan memory */
    void Unload();

    /** @brief Dapatkan pointer ke internal TilesonMapData */
    TilesonMapData *GetData() const;

    /**
     * @brief Buat prefab baru hasil rotasi dari prefab ini
     * @param tileDegrees Rotasi tile layer (0, 90, 180, 270)
     * @param objectDegrees Rotasi object layer non-obstacle (0, 90, 180, 270)
     * @return WorldGenPrefab baru berisi hasil rotasi (ownership terpisah)
     */
    WorldGenPrefab Rotate(int tileDegrees, int objectDegrees);

private:
    TilesonMapData *data; // Pointer ke data tilemap hasil load
};

/*==============================================================================
 * Class: WorldGenCanvas
 *==============================================================================*/

/**
 * @brief Operasi stamping prefab dan corridor ke main map canvas
 *
 * WorldGenCanvas bekerja pada tilesonMap global dan membutuhkan
 * WorldGenPools untuk akses ke corridor pool saat stamp corridor.
 */
class WorldGenCanvas
{
public:
    /**
     * @param map Pointer ke main map canvas (biasanya tilesonMap global)
     * @param pools Pointer ke instance WorldGenPools
     * @param seed Seed untuk deterministic RNG (harus sama dengan WorldGenLayout)
     */
    WorldGenCanvas(TilesonMapData *map, WorldGenPools *pools, uint64_t seed);

    /**
     * @brief Ambil semua slot worldgen dari layer "slot_worldgen"
     * @return Vector berisi MapObject untuk setiap slot yang ditemukan
     */
    std::vector<MapObject> GetSlots();

    /**
     * @brief Tambah layer baru ke canvas
     * @param extraLayers Jumlah layer yang akan ditambah
     */
    void ExpandLayers(int extraLayers);

    /**
     * @brief Stamp source map ke canvas pada posisi tertentu
     * @param source Map yang akan di-stamp
     * @param offsetX Offset X dalam tile
     * @param offsetY Offset Y dalam tile
     * @param targetLayerOffset Layer awal untuk stamp
     */
    void Stamp(TilesonMapData *source, int offsetX, int offsetY, int targetLayerOffset);

    /**
     * @brief Stamp prefab ke slot worldgen (auto-center)
     * @param prefab Prefab yang akan di-stamp
     * @param slot Slot target
     */
    void StampToSlot(TilesonMapData *prefab, MapObject *slot);

    /**
     * @brief Stamp corridor dari exit object ke border slot
     * @param exitObj Object exit (exit_north/south/east/west)
     * @param slotCol Kolom slot dalam grid worldgen
     * @param slotRow Baris slot dalam grid worldgen
     */
    void StampCorridor(const MapObject &exitObj, int slotCol, int slotRow);

    /**
     * @brief Stamp seluruh layout ke canvas
     * @param grid Layout grid worldgen
     * @param slots Daftar slot dari canvas
     */
    void StampLayout(const std::vector<std::vector<WorldCell>> &grid,
                     const std::vector<MapObject> &slots);

private:
    TilesonMapData *tilesonMap; // Main map canvas — reference ke global
    WorldGenPools *pools;       // Reference ke pools untuk akses corridor
    std::mt19937_64 wgRng;      // RNG deterministic dari worldSeed
    static int RotateExitMask(int mask, int degrees);
    WeightedPool *SelectPool(CellType type, int exitMask, RoomPool &roomPool, int &outBaseMask);
    WorldGenPrefab ResolveRotation(CellType type, int exitMask, std::mt19937_64 &rng);
};

/**
 * @brief Layout generator untuk world grid dungeon
 *
 * Menghasilkan layout 4x4 grid menggunakan algoritma Prim's
 * untuk menentukan konektivitas room, lalu assign tiap cell
 * dengan tipe tertentu (enemy, treasure, boss, dll).
 */
class WorldGenLayout
{
public:
    /**
     * @brief Constructor layout generator
     * @param seed Seed deterministic
     * @param isBossStage True jika stage ini adalah boss stage
     */
    WorldGenLayout(uint64_t seed, bool isBossStage = false);

    /** @brief Generate layout grid */
    void Generate();

    /** @brief Dapatkan grid hasil generate */
    const std::vector<std::vector<WorldCell>> &GetGrid() const;

private:
    std::mt19937 wgRng;
    std::vector<std::vector<WorldCell>> grid; // 4x4
    bool isBoss = false;

    void InitGrid();                                          // Init semua cell ke CELL_EMPTY
    void RunPrims();                                          // Generate konektivitas room dengan Prim's algorithm
    void AssignCellTypes();                                   // Assign tipe ke tiap cell sesuai aturan
    void PruneSingleExitCells();                              // Hapus cell yang cuma punya 1 exit
    void RemoveDisconnectedCells();                           // Hapus cell yang tidak terhubung
    void ConstrainExitsByPool();                              // Sesuaikan exit mask dengan pool yang tersedia
    void EnsureTreasureExists();                              // Pastikan minimal ada 1 treasure room
    void PruneOneDenseExit();                                 // Prune satu exit dari cell yang terlalu padat
    void DebugPrintGrid() const;                              // Print grid ke console (debug)
    bool IsLeaf(int r, int c) const;                          // Cek apakah cell adalah leaf node
    bool IsAdjacentToType(int r, int c, CellType type) const; // Cek apakah cell bertetangga dengan tipe tertentu
    int CountActiveCells() const;                             // Hitung jumlah cell aktif di grid
    std::vector<std::vector<int>> ComputeDepth() const;       // Hitung depth tiap cell dari start
};

/*==============================================================================
 * Seed Functions
 *==============================================================================*/

/** @brief Generate seed random untuk satu run game */
uint64_t GenerateRunSeed(void);

/** @brief Dapatkan seed run yang sedang aktif */
uint64_t GetCurrentRunSeed(void);

/*==============================================================================
 * Worldgen Grid Lookup — untuk stage transition detection
 *==============================================================================*/

struct WorldgenCellInfo
{
    CellType type;
    Rectangle bounds;
};

extern WorldgenCellInfo g_worldgenCells[WG_GRID_SIZE * WG_GRID_SIZE];
extern int g_worldgenCellCount;

/** @brief Init worldgen grid lookup dari layout + slots */
void InitWorldgenGrid(const std::vector<std::vector<WorldCell>> &grid,
                      const std::vector<MapObject> &slots);
/** @brief Cari tipe cell berdasarkan posisi world */
CellType GetCellTypeAtWorldPos(Vector2 worldPos);
/** @brief Cari index cell grid berdasarkan posisi world */
int GetCellIndexAtWorldPos(Vector2 worldPos);
/** @brief Cek apakah posisi world berada di cell dengan tipe tertentu */
bool IsCellTypeAtWorldPos(Vector2 worldPos, CellType type);
