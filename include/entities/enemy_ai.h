#pragma once

#include "../lib/raylib/include/raylib.h"
#include "map.h"
#include "mapLogic.h"
#include "animation.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>

/**
 * @file enemy_ai.h
 * @brief Enemy AI, Flow Field & Steering Module
 *
 * Header ini mendeklarasikan sistem AI untuk enemy:
 * flow field pathfinding, steering behavior, obstacle cache,
 * spatial hash untuk separation, dan global flow field.
 */

class Enemy;

/*==============================================================================
 * FlowField
 *==============================================================================*/
constexpr int FLOW_FIELD_TILE_SIZE = FRAME_SIZE;                        // ukuran tile flow field, mengikuti FRAME_SIZE map
constexpr float FLOW_FIELD_CENTER_OFFSET = FLOW_FIELD_TILE_SIZE * 0.5f; // offset dari pojok tile ke pusat tile
constexpr float FLOW_FIELD_REBUILD_COOLDOWN = 0.3f;                     // jeda minimum antar rebuild flow field
constexpr int FLOW_FIELD_PLAYER_RADIUS = 10;                            // radius area aktif flow field player dalam tile
constexpr int FLOW_FIELD_RETURN_RADIUS = 18;                            // radius area aktif flow field return dalam tile
constexpr int STEERING_GRID_RADIUS = 2;                                 // radius grid evaluasi steering di sekitar enemy

constexpr float FLOW_FIELD_OBSTACLE_PENALTY = 5.f; // cost tambahan untuk tile di dekat obstacle
constexpr float FLOW_FIELD_DIAGONAL_COST = 1.414f; // cost gerak diagonal, mendekati sqrt(2)
constexpr float FLOW_FIELD_CARDINAL_COST = 1.0f;   // cost gerak horizontal/vertikal

static constexpr float SEPARATION_RADIUS = 28.0f;          // jarak maksimum antar enemy untuk mulai saling menjauh
static constexpr float SEPARATION_STRENGTH = 25.0f;        // besar dorongan separation antar enemy
static constexpr float MAX_SEPARATION_FORCE = 30.f;        // batas maksimum gaya separation
static constexpr int CELL_SIZE = FRAME_SIZE * 1.8f;        // ukuran cell spatial hash untuk query neighbor enemy
static constexpr float SEPARATION_FORCE_MAGNITUDE = 0.03f; // besaran nilai interpolasi untuk separation force

/**
 * @brief Grid arah menuju player, dihitung sekali dan dibaca semua enemy.
 *
 * Cara kerja:
 * - Build() dipanggil saat player pindah tile atau map baru di-load
 * - GetDirection() dipanggil tiap enemy tiap frame — O(1), tidak ada pathfinding per enemy
 *
 * Koordinat grid: tileX = worldX / FLOW_FIELD_TILE_SIZE
 */
class FlowField
{
public:
    /**
     * @brief Build flow field dari posisi player sebagai tujuan.
     * @param goalWorld Posisi player dalam world space
     * @param mapWidth Lebar map dalam satuan tile
     * @param mapHeight Tinggi map dalam satuan tile
     * @note Pakai BFS dari goal — tile non-walkable di-skip
     */
    void Build(Vector2 goalWorld, int mapWidth, int mapHeight, int radius);

    /**
     * @brief Ambil arah gerak untuk posisi world space tertentu.
     * @param worldPos Posisi enemy dalam world space
     * @return Arah normalized menuju player, {0,0} jika tile tidak valid atau tidak reachable
     */
    Vector2 GetDirection(Vector2 worldPos) const;

    /** @brief Cek apakah flow field sudah siap dipakai */
    bool IsReady() const { return isReady_; }

    /** @brief Notify map baru di-load — paksa rebuild build berikutnya */
    void Invalidate();

    /**
     * @brief Update dipanggil tiap frame — cek apakah perlu rebuild berdasarkan posisi player.
     * @param playerWorld Posisi player dalam world space
     * @param mapWidth Lebar map dalam satuan tile
     * @param mapHeight Tinggi map dalam satuan tile
     */
    void Update(Vector2 playerWorld, int mapWidth, int mapHeight);

    /**
     * @brief Ambil cost traversal tile pada posisi world space tertentu.
     * @param worldPos Posisi dalam world space yang akan dicek
     * @return Cost tile, atau FLT_MAX jika tile tidak valid atau tidak reachable
     */
    float GetCost(Vector2 worldPos) const;

private:
    struct Cell
    {
        Vector2 direction = {0, 0}; // arah normalized menuju goal
        bool walkable = false;      // false jika tile ini obstacle
        bool reached = false;       // false jika tile tidak reachable dari goal
        float cost = 1.f;           // cost traversal tile, termasuk penalty obstacle
    };

    std::vector<std::vector<Cell>> grid_; // grid flow field dengan akses grid_[y][x]
    int gridWidth_ = 0;                   // lebar grid dalam tile
    int gridHeight_ = 0;                  // tinggi grid dalam tile
    bool isReady_ = false;                // status kesiapan flow field setelah build
    bool hasAllocatedGrid_ = false;       // true setelah grid dialokasikan pertama kali

    // throttle & dirty tracking
    Vector2 lastGoalTile_ = {-1, -1}; // tile terakhir saat build dilakukan
    float rebuildCooldown_ = 0.f;     // sisa waktu cooldown rebuild

    bool IsValidTile(int x, int y) const;                                            // Cek apakah koordinat tile valid
    void Dijkstra(int goalX, int goalY, int startX, int startY, int endX, int endY); // Hitung jarak terpendek tiap tile
    float ComputeTileCost(int x, int y);                                             // Hitung cost traversal tile
};

/*==============================================================================
 * EnemySteering
 *==============================================================================*/
/** @brief Mode steering enemy */
enum SteeringMode
{
    STEERING_CHASE, // steering menuju player memakai global flow field
    STEERING_RETURN // steering kembali ke spawn memakai return flow field
};

/** @brief Konteks steering untuk satu frame */
struct SteeringContext
{
    Vector2 Position;                           // posisi enemy saat ini
    Vector2 Velocity;                           // velocity enemy dari frame sebelumnya
    float HitBoxValue;                          // ukuran hitbox untuk validasi pathfinding
    float OffsetValue;                          // offset hitbox untuk validasi pathfinding
    float TileCenterOffset;                     // offset pusat tile untuk sampling posisi
    float DetectionRange;                       // jarak deteksi aktif enemy
    float rayLength;                            // panjang raycast obstacle steering
    float rayDetectionLength;                   // radius deteksi langsung ke player
    Vector2 PlayerCenter;                       // posisi pusat player
    Rectangle PlayerHitbox;                     // hitbox player untuk cek range
    Vector2 SpawnPoint;                         // titik spawn enemy
    const FlowField *ReturnFlowField = nullptr; // flow field untuk kembali ke spawn
};

/** @brief Helper steering untuk chase dan return */
class EnemySteering
{
public:
    /** @brief Hitung arah steering berdasarkan mode chase/return dan obstacle raycast */
    Vector2 Compute(SteeringMode mode, const SteeringContext &ctx, RayCast &ray);

    /** @brief Cek apakah player dalam radius deteksi langsung enemy */
    bool IsPlayerInRange(const SteeringContext &ctx);

    /** @brief Helper debug untuk cek overlap range enemy ke hitbox player */
    bool IsInRangeDebug(Vector2 enemyCenter, Rectangle playerHitbox, float rayLength);

    Vector2 SteeringDir = {0, 0};        // arah steering terakhir yang dipakai
    Vector2 LastFlowTile = {-1, -1};     // tile flow field terakhir saat steering dihitung
    Vector2 SteeringTarget = {0.f, 0.f}; // target world space hasil evaluasi steering

    int SteeringFlipCount = 0;            // jumlah perubahan arah berlawanan dalam window timer
    int MaxSteeringFlipCount = 4;         // batas flip sebelum enemy keluar dari mode steering aktif
    float SteeringFlipTimer = 0.f;        // sisa waktu window deteksi flip arah
    float SteeringFlipTimerWindow = 0.3f; // durasi window deteksi flip arah
    float SteeringCooldown = 0.f;         // sisa cooldown sebelum steering dievaluasi ulang
    float SteeringCooldownWindow = 0.3f;  // durasi cooldown update steering
    float ScoreMultiplier = 0.9f;         // bobot momentum arah lama saat scoring steering

private:
    Vector2 EvaluateGrid(const SteeringContext &ctx, Vector2 flowDir, SteeringMode mode); // Pilih arah terbaik dari grid kandidat
    void ApplyAntiFlip(Vector2 bestDir, Vector2 prevDir);                                 // Catat perubahan arah untuk cegah bolak-balik
};

/*==============================================================================
 * Obstacle Cache
 *==============================================================================*/
/** @brief Bangun daftar obstacle dari map */
std::vector<MapObject> BuildObstacleList();

extern std::vector<MapObject> cachedObstacleList; // cache obstacle untuk raycast steering dan serangan

/** @brief Rebuild cache obstacle */
void RebuildObstacleCache();

/*==============================================================================
 * Spawn Flow Fields
 *==============================================================================*/
/** @brief Interval pencarian ulang return flow field saat enemy pulang */
const float RETURN_SCAN_INTERVAL = 2.0f;

/** @brief Entry spawn dengan flow field return */
struct SpawnFlowFieldEntry
{
    Vector2 spawnPos; // posisi spawn atau pusat area spawn
    FlowField field;  // flow field return menuju spawnPos
};

/** @brief Map flow field return per ID object spawn */
extern std::unordered_map<int, SpawnFlowFieldEntry> spawnFlowFields;
/** @brief Antrean ID spawn yang perlu rebuild flow field */
extern std::queue<int> spawnFlowFieldRebuildQueue;

/** @brief Bangun flow field untuk spawn tertentu */
void BuildSpawnFlowFields(Vector2 spawnPos, int objId, int mapWidth, int mapHeight);
/** @brief Temukan flow field spawn terdekat */
FlowField *FindNearestSpawnFlowField(Vector2 position);
/** @brief Tandai spawn flow field sebagai dirty */
void MarkSpawnFlowFieldsDirty(Vector2 position);

/*==============================================================================
 * SpatialHash
 *==============================================================================*/
/** @brief Spatial hash untuk query neighbor enemy */
struct SpatialHash
{
    std::unordered_map<uint64_t, std::vector<int>> cells; // daftar index enemy per cell spatial hash

    /** @brief Buat key 64-bit dari koordinat cell spatial hash */
    uint64_t Key(int cellX, int cellY)
    {
        return ((uint64_t)(uint32_t)cellX << 32) | (uint32_t)cellY;
    }

    /** @brief Kosongkan semua data cell */
    void Clear() { cells.clear(); }

    /** @brief Masukkan index enemy ke cell berdasarkan posisi */
    void Insert(int index, Vector2 pos)
    {
        int cx = (int)std::floor(pos.x / CELL_SIZE);
        int cy = (int)std::floor(pos.y / CELL_SIZE);
        cells[Key(cx, cy)].push_back(index);
    }

    /**
     * @brief Ambil semua index enemy di cell sekitar posisi tertentu.
     * @param pos Posisi world space yang menjadi pusat query
     * @return Daftar index enemy dari 3x3 cell di sekitar posisi
     * @note Hasil belum difilter berdasarkan jarak aktual atau status aktif enemy.
     */
    std::vector<int> Query(Vector2 pos)
    {
        int cx = (int)std::floor(pos.x / CELL_SIZE);
        int cy = (int)std::floor(pos.y / CELL_SIZE);
        std::vector<int> result;
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
            {
                auto it = cells.find(Key(cx + dx, cy + dy));
                if (it != cells.end())
                    for (int idx : it->second)
                        result.push_back(idx);
            }
        return result;
    }
};

/** @brief Rebuild spatial hash dari daftar enemy */
void RebuildSpatialHash(std::vector<Enemy *> &enemies);
/** @brief Hitung separation force untuk enemy tertentu */
Vector2 CalcSeparationForce(int index, std::vector<Enemy *> &enemies);

/*==============================================================================
 * Globals
 *==============================================================================*/
/** @brief Flow field global untuk chase */
extern FlowField globalFlowField;