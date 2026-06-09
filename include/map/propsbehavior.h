#pragma once

/**
 * @file propsbehavior.h
 * @brief Props & Trap Behavior System
 *
 * Header ini mendeklarasikan semua manager untuk interactable props dan trap:
 * - ChestManager: chest yang bisa dibuka player untuk loot item
 * - SpikeManager: trap spike dengan timer aktif/nonaktif dan damage area
 * - BombManager: trap bomb yang meledak saat dipukul, dengan chain reaction
 * - CrateManager: crate yang bisa dihancurkan player/bomb dan punya chance drop loot
 * - SignManager: spawn dan interaksi sign yang bisa dibaca player
 */

#include "map.h"
#include "mapLogic.h"
#include "animation.h"
#include "player.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/raylib/include/raymath.h"
#include <random>
#include <unordered_set>
#include <string>

/*==============================================================================
 * ObjectState Enum
 *==============================================================================*/

/** @brief State universal untuk semua tile object */
enum class ObjectState
{
    Closed,  // Chest belum dibuka
    Open,    // Chest sudah dibuka
    Active,  // Trap sedang aktif
    Inactive // Trap sedang tidak aktif
};

/*==============================================================================
 * TileObject Struct
 *==============================================================================*/

/** @brief Representasi universal untuk semua props dan trap di map */
struct TileObject
{
    Vector2 position;  // Posisi final setelah snap ke tile grid
    Rectangle bounds;  // Bounding box asli dari MapObject Tiled
    ObjectState state; // State saat ini
    std::string name;  // Nama object dari Tiled untuk identifikasi
    std::string uuid;  ///< Unique identifier for persistent entity matching across save/load cycles. Generated at spawn time, persisted in save files, used for restore matching.
};

/*==============================================================================
 * SpawnObject
 *==============================================================================*/

/**
 * @brief Spawn semua object dari Tiled ke manager masing-masing
 *
 * Entry point yang dipanggil saat map selesai di-load.
 */
void SpawnObject(void);

/**
 * @brief Trigger hit attack player ke props yang bisa dihancurkan.
 * @param attackHitbox Hitbox serangan player
 * @param playerBounds Bounding box player untuk efek props tertentu
 * @param player Pointer ke player
 */
void HitPropsByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player);

/**
 * @brief Encode posisi world space menjadi key string untuk tracking object.
 * @param pos Posisi object dalam world space
 * @return String key berbentuk "x_y"
 */
inline std::string EncodePos(Vector2 pos)
{
    return std::to_string((int)pos.x) + "_" + std::to_string((int)pos.y);
}

/*==============================================================================
 * ChestManager
 *==============================================================================*/

/**
 * @brief Manager untuk semua chest di map
 *
 * Menangani spawn, interaksi player, dan distribusi loot.
 */
class ChestManager
{
public:
    /** @brief Spawn semua chest dari object layer Tiled */
    void SpawnChests(const std::vector<MapObject *> &chestObjects);

    /** @brief Trigger interaksi player dengan chest terdekat */
    void Interact(Vector2 hitPos);

    /** @brief Render semua chest (placeholder sprite) */
    int Render(Rectangle viewRect);

    /** @brief Bersihkan semua data chest */
    void Clear();

    /** @brief Reset posisi chest yang sudah dikonsumsi (untuk new game) */
    void ResetConsumed();

    /** @brief Ambil posisi chest yang sudah dibuka */
    const std::unordered_set<std::string> &GetConsumedPositions() const { return consumedPositions; }
    /** @brief Set posisi chest yang sudah dibuka (buat load) */
    void SetConsumedPositions(const std::unordered_set<std::string> &positions) { consumedPositions = positions; }

    /**
     * @brief Ambil jumlah chest yang sedang dikelola.
     * @return Jumlah chest aktif di manager
     */
    size_t GetCount() const { return chests.size(); }

private:
    std::vector<TileObject> chests;                    // Daftar chest yang sedang dikelola
    std::unordered_set<std::string> consumedPositions; // Posisi chest yang sudah dikonsumsi agar tidak diproses ulang
    float chestSpread = 60.0f;                         // Radius sebaran loot dari chest

    TileObject *FindChest(Vector2 hitPos, float threshold = 32.0f); // Cari chest terdekat dari titik hit
    void TriggerLoot(TileObject &chest);                            // Spawn loot item secara random di sekitar chest
};

/*==============================================================================
 * SpikeManager
 *==============================================================================*/

/** @brief Callback untuk event spike */
using SpikeCallback = std::function<void(TileObject &)>;

/**
 * @brief Manager untuk semua trap spike di map
 *
 * Tiap spike punya durasi aktif/nonaktif yang di-randomisasi saat spawn.
 * Damage ke player dan enemy menggunakan cooldown global.
 */
class SpikeManager
{
public:
    /** @brief Spawn semua spike dari object layer Tiled */
    void SpawnSpikes(const std::vector<MapObject *> &spikeObjects);

    /**
     * @brief Update timer dan damage spike tiap frame
     * @param deltaTime Waktu antar frame
     * @param playerBounds Bounding box player
     * @param player Pointer ke player untuk apply damage
     */
    void Update(float deltaTime, Rectangle playerBounds, Player *player);

    /** @brief Render semua spike (placeholder sprite) */
    int Render(Rectangle viewRect);

    /** @brief Bersihkan semua data spike */
    void Clear();

    /** @brief Ambil jumlah spike yang sedang dikelola */
    size_t GetCount() const { return spikes.size(); }

    // Konstanta timing dan damage spike (public untuk unit test)
    static constexpr float SPIKE_ACTIVE_MAX = 6.0f;      // Durasi aktif maksimum (detik)
    static constexpr float SPIKE_ACTIVE_MIN = 3.0f;      // Durasi aktif minimum (detik)
    static constexpr float SPIKE_INACTIVE_MAX = 7.0f;    // Durasi nonaktif maksimum (detik)
    static constexpr float SPIKE_INACTIVE_MIN = 4.0f;    // Durasi nonaktif minimum (detik)
    static constexpr float SPIKE_DAMAGE = 10.0f;         // Damage per hit
    static constexpr float SPIKE_DAMAGE_COOLDOWN = 1.0f; // Cooldown damage global (detik)

private:
    /** @brief Data internal satu spike */
    struct SpikeData
    {
        TileObject tile;
        float activeTimer;      // Sisa waktu aktif
        float inactiveTimer;    // Sisa waktu nonaktif
        float activeDuration;   // Total durasi aktif (randomized)
        float inactiveDuration; // Total durasi nonaktif (randomized)
        float damageCooldown;   // Cooldown damage per spike (unused, pakai global)
        SpikeCallback onActivate;
        SpikeCallback onDeactivate;
        SpikeCallback onDamagePlayer;
    };

    float globalPlayerDamageCooldown = 0.0f; // Cooldown damage ke player (shared semua spike)
    float globalEnemyDamageCooldown = 0.0f;  // Cooldown damage ke enemy (shared semua spike)

    std::vector<SpikeData> spikes; // Daftar spike yang sedang dikelola

    unsigned int SeedFromName(const std::string &name); // Generate seed dari nama object untuk randomisasi timer
    void SetupCallbacks(SpikeData &spike);              // Setup callback onActivate, onDeactivate, onDamagePlayer
};

/*==============================================================================
 * BombManager
 *==============================================================================*/

/**
 * @brief Manager untuk semua trap bomb di map
 *
 * Bomb meledak saat terkena serangan player. Ledakan mengenai player,
 * enemy, dan bomb lain dalam radius (chain reaction).
 */
class BombManager
{
public:
    /** @brief Spawn semua bomb dari object layer Tiled */
    void SpawnBombs(const std::vector<MapObject *> &bombObjects);

    /**
     * @brief Trigger ledakan bomb yang terkena hitbox serangan player
     * @param attackHitbox Hitbox serangan player
     * @param playerBounds Bounding box player
     * @param player Pointer ke player
     */
    void HitByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player);

    /**
     * @brief Update state semua bomb tiap frame
     * @param deltaTime Waktu antar frame
     * @param playerBounds Bounding box player
     * @param player Pointer ke player
     */
    void Update(float deltaTime, Rectangle playerBounds, Player *player);

    /** @brief Render semua bomb (placeholder sprite) */
    int Render(Rectangle viewRect);

    /** @brief Bersihkan semua data bomb */
    void Clear();

    /** @brief Reset posisi bomb yang sudah meledak (untuk new game) */
    void ResetConsumed();

    /** @brief Ambil jumlah bomb yang sedang dikelola */
    size_t GetCount() const { return bombs.size(); }

    /**
     * @brief Dapatkan posisi bomb yang sudah meledak (consumed).
     * @return Const reference ke unordered_set posisi yang sudah dikonsumsi
     */
    const std::unordered_set<std::string>& GetConsumedPositions() const { return consumedPositions; }

    /**
     * @brief Set posisi bomb yang sudah meledak (untuk restore save state).
     * @param positions Set posisi yang sudah dikonsumsi
     */
    void SetConsumedPositions(const std::unordered_set<std::string>& positions) { consumedPositions = positions; }

    /**
     * @brief Cek apakah target berada dalam radius ledakan
     *
     * Pakai nearest-point check, bukan center-to-center.
     *
     * @param bombPos Posisi center bomb
     * @param target Bounding box target
     * @return true jika jarak nearest point <= BOMB_EXPLOSION_RADIUS
     */
    bool IsInExplosionRadius(Vector2 bombPos, Rectangle target);

    // Konstanta bomb (public untuk unit test)
    static constexpr float BOMB_EXPLOSION_RADIUS = 80.0f;  // Radius area ledakan (pixel)
    static constexpr float BOMB_DAMAGE = 25.0f;            // Damage ledakan
    static constexpr float BOMB_EXPLOSION_DURATION = 0.6f; // Durasi animasi ledakan (detik)

private:
    /** @brief Data internal satu bomb */
    struct BombData
    {
        TileObject tile;
        bool isAlive;             // False jika sudah selesai meledak
        bool isExploding;         // True selama animasi ledakan berlangsung
        bool isTriggered = false; // Guard untuk mencegah infinite loop chain reaction
        float explosionTimer;     // Sisa waktu animasi ledakan
    };

    std::vector<BombData> bombs;                       // Daftar bomb yang sedang dikelola
    std::unordered_set<std::string> consumedPositions; // Posisi bomb yang sudah dikonsumsi agar tidak diproses ulang
    Player *playerRef = nullptr;                       // Referensi player terakhir untuk update/interaction bomb

    TileObject *FindBomb(Vector2 hitPos, float threshold = 32.0f); // Cari bomb terdekat dari titik hit

    void Explode(BombData &bomb, Rectangle playerBounds, Player *player); // Trigger ledakan, damage area, dan chain reaction
};

/*==============================================================================
 * CrateManager
 *==============================================================================*/

/**
 * @brief Manager untuk semua crate di map.
 *
 * Crate bisa dihancurkan oleh serangan player atau ledakan bomb,
 * lalu punya peluang menjatuhkan loot.
 */
class CrateManager
{
public:
    /** @brief Spawn semua crate dari object layer Tiled */
    void SpawnCrates(const std::vector<MapObject *> &crateObjects);
    /** @brief Hancurkan crate yang terkena hitbox serangan player */
    void HitByAttack(Rectangle attackHitbox);
    /** @brief Hapus crate yang sudah tidak aktif dari daftar runtime */
    void Update();
    /** @brief Hancurkan crate yang terkena radius ledakan bomb */
    void HitByExplosion(Vector2 bombPos, BombManager *bomber);
    /** @brief Render crate yang terlihat dalam view */
    int Render(Rectangle viewRect);
    /** @brief Bersihkan semua data crate */
    void Clear();

    /** @brief Ambil posisi crate yang sudah hancur */
    const std::unordered_set<std::string> &GetConsumedPositions() const { return consumedPositions; }
    /** @brief Set posisi crate yang sudah hancur (buat load) */
    void SetConsumedPositions(const std::unordered_set<std::string> &positions) { consumedPositions = positions; }

    /** @brief Reset posisi crate yang sudah hancur (untuk new game) */
    void ResetConsumed();

    /**
     * @brief Ambil jumlah crate yang sedang dikelola.
     * @return Jumlah crate aktif di manager
     */
    size_t GetCount() const { return crates.size(); }

    static constexpr float CRATE_LOOT_CHANCE = 0.1f; // 10% chance drop loot (public untuk unit test)

private:
    /** @brief Data internal satu crate */
    struct CrateData
    {
        TileObject tile; // Data object crate di map
        bool isAlive;    // False jika crate sudah dihancurkan
    };

    std::vector<CrateData> crates;                     // Daftar crate yang sedang dikelola
    std::unordered_set<std::string> consumedPositions; // Posisi crate yang sudah dihancurkan agar tidak spawn ulang
    float crateSpread = 60.0f;                         // Radius sebaran loot dari crate

    TileObject *FindCrate(Vector2 hitPos, float threshold = 32.0f); // Cari crate terdekat dari titik hit
    void Destroy(CrateData &crate);                                 // Hancurkan crate, hapus obstacle, dan trigger loot
    void TriggerLoot(TileObject &crate);                            // Roll peluang drop loot dari crate
};

/*==============================================================================
 * BarrierManager
 *==============================================================================*/

/**
 * @brief Manager untuk barrier door yang nutup entrance room.
 *
 * Barrier aktif pas room dimuat, ngeblok entrance via DynamicObstacles.
 * Hilang setelah kill threshold terpenuhi (default 90%).
 * Di boss room: barrier buka → re-lock pas player masuk → buka lagi setelah boss mati.
 */
class BarrierManager
{
public:
    /** @brief Spawn semua barrier dari object layer Tiled */
    void SpawnBarriers(const std::vector<MapObject *> &barrierObjects);
    /** @brief Update tiap frame — cek kill threshold & boss room state */
    void Update();
    /** @brief Render barrier sebagai rectangle berwarna */
    int Render(Rectangle viewRect);
    /** @brief Bersihkan semua data barrier */
    void Clear();

    /** @brief Ambil jumlah barrier yang sedang dikelola */
    size_t GetCount() const { return barriers.size(); }
    /** @brief Apakah barrier sudah pernah di-clear */
    bool IsCleared() const { return cleared; }
    /** @brief Apakah player sudah masuk boss room (re-lock aktif) */
    bool HasReLocked() const { return hasReLocked; }
    /** @brief Set state cleared */
    void SetCleared(bool v) { cleared = v; }
    /** @brief Set state hasReLocked */
    void SetHasReLocked(bool v) { hasReLocked = v; }

    static constexpr float KILL_THRESHOLD = 0.9f;

private:
    /** @brief Data internal satu barrier */
    struct BarrierData
    {
        TileObject tile;
        bool isActive;
        bool isBoss = false; // true kalo dari type "barrier_boss"
    };

    std::vector<BarrierData> barriers; // Daftar barrier yang sedang dikelola
    bool isBossMap = false;            // True kalo map ini punya boss spawn
    bool hasReLocked = false;          // True setelah player masuk room boss dan barrier re-lock
    bool cleared = false;              // True kalo barrier udah pernah di-clear
    int totalEnemyCount = 0;           // Total enemy di EnemyRegistry pas spawn
    int prevDeadCount = 0;             // DeadCount sebelumnya — buat deteksi perubahan
    Rectangle bossStageBounds = {0};   // Bounds object "boss_stage" untuk deteksi area boss

    void RemoveAllBarriers(); // Hapus semua barrier dari DynamicObstacles
    void ReLockBarriers();    // Pasang ulang barrier (re-lock) — khusus boss room
};

/*==============================================================================
 * SignManager
 *==============================================================================*/

/**
 * @brief Manager untuk semua sign yang bisa dibaca player
 *
 * Sign adalah object interaktif di map yang saat di-raycast oleh player
 * akan menampilkan teks dialog. Teks diambil dari custom property "dialog"
 * di Tiled.
 */
class SignManager
{
public:
    /** @brief Spawn semua sign dari object layer Tiled */
    void SpawnSigns(const std::vector<MapObject *> &signObjects);

    /** @brief Interaksi player dengan sign yang terkena raycast */
    void Interact(Vector2 hitPos);

    /** @brief Render placeholder sign (DARKGREEN rectangle) */
    int Render(Rectangle viewRect);

    /** @brief Bersihkan semua data sign */
    void Clear();

    /** @brief Apakah dialog sign sedang aktif */
    bool IsDialogActive() const { return isDialogActive; }
    /** @brief Ambil baris dialog yang aktif */
    const std::vector<std::string> &GetActiveDialogLines() const { return activeDialogLines; }
    /** @brief Tutup dialog */
    void DismissDialog();

private:
    /** @brief Data internal satu sign */
    struct SignData
    {
        TileObject tile;                // Posisi, bounds, state
        std::vector<std::string> lines; // Baris dialog hasil split (| atau \n)
    };

    std::vector<SignData> signs; // Daftar sign yang sedang dikelola

    bool isDialogActive = false;
    std::vector<std::string> activeDialogLines;

    /** @brief Split teks dialog jadi baris-baris berdasarkan | atau \n */
    static std::vector<std::string> SplitDialog(const std::string &text);

    /** @brief Cari sign terdekat dari titik hit */
    SignData *FindSign(Vector2 hitPos, float threshold = 32.0f);
};

/*==============================================================================
 * Global Manager Instances
 *==============================================================================*/

/** @brief Instance global manager chest */
extern ChestManager chestManager;
/** @brief Instance global manager spike */
extern SpikeManager spikeManager;
/** @brief Instance global manager bomb */
extern BombManager bombManager;
/** @brief Instance global manager crate */
extern CrateManager crateManager;
/** @brief Instance global manager barrier */
extern BarrierManager barrierManager;
/** @brief Instance global manager sign */
extern SignManager signManager;
