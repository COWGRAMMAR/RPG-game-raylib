#pragma once

#include "entity.h"
#include "raylib.h"
#include "animation.h"
#include "mapLogic.h"
#include "enemy_ai.h"
#include "player.h"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @file enemy.h
 * @brief Enemy System Module
 *
 * Header ini mendeklarasikan data, enum, struct, dan kelas untuk
 * sistem enemy: definisi data-driven, spawn, AI, dan manajemen.
 */

/** @brief Spawn count range constants */
constexpr int SPAWN_PINPOINT_NORMAL_MIN = 9;
constexpr int SPAWN_PINPOINT_NORMAL_MAX = 13;
constexpr int SPAWN_PINPOINT_ELITE_MIN = 3;
constexpr int SPAWN_PINPOINT_ELITE_MAX = 7;
constexpr int SPAWN_RECT_NORMAL_MIN = 20;
constexpr int SPAWN_RECT_NORMAL_MAX = 25;
constexpr int SPAWN_RECT_ELITE_MIN = 10;
constexpr int SPAWN_RECT_ELITE_MAX = 15;
constexpr int SPAWN_RETRY_LIMIT = 200;

// --- Loot drop tunable constants ---
constexpr float LOOT_DROP_CHANCE_NORMAL = 0.25f;
constexpr float LOOT_DROP_CHANCE_ELITE  = 0.50f;
constexpr int LOOT_RARITY_COMMON   = 75;
constexpr int LOOT_RARITY_UNCOMMON = 25;
constexpr int LOOT_RARITY_ELITE_UNCOMMON = 70;
constexpr int LOOT_RARITY_ELITE_RARE     = 30;

/*==============================================================================
 * Enums
 *==============================================================================*/

/** @brief Status AI untuk perilaku musuh (FSM) */
enum EnemyAIState
{
    ENEMY_IDLE,     // Berdiri diam atau menunggu
    ENEMY_PATROL,   // Bergerak antar titik acak di sekitar titik spawn
    ENEMY_CHASE,    // Mengejar pemain
    ENEMY_ATTACK,   // Menjalankan animasi/logika serangan
    ENEMY_RETURN,   // Kembali ke titik spawn setelah kehilangan pemain
    ENEMY_ABILITY1, // Menjalankan animasi/logika serangan khusus
    ENEMY_ABILITY2  // Menjalankan animasi/logika serangan khusus
};

/** @brief Mode raycast untuk deteksi LoS enemy */
enum RayCastMode
{
    LINE, // Satu garis lurus
    CONE  // Berbentuk cone untuk deteksi lebih lebar
};

/** @brief Rank enemy untuk balancing */
typedef enum EnemyRank
{
    ENEMY_NORMAL, // Enemy biasa
    ENEMY_ELITE,  // Enemy elite
    ENEMY_BOSS    // Enemy boss
};

/*==============================================================================
 * Data Structs (Data-Driven)
 *==============================================================================*/

/** @brief Statistik dan parameter gameplay enemy, di-load dari JSON */
struct EnemyStats
{
    float maxHealth;             // Batas HP maksimum
    float speed;                 // Kecepatan gerak saat patroli/kembali ke spawn
    float chaseSpeed;            // Kecepatan gerak saat mengejar pemain
    float damage;                // Damage per serangan
    float baseDetectionRange;    // Jarak deteksi saat idle/patroli
    float chaseDetectionRange;   // Jarak deteksi saat mengejar (lebih sulit kabur)
    float attackRange;           // Jarak minimum untuk memicu serangan
    float healthRegenRate;       // HP yang pulih per detik saat di luar pertempuran
    float healthRegenDelay;      // Jeda (detik) setelah terkena damage sebelum regen aktif
    float patrolRadius;          // Radius maksimum patroli dari titik spawn
    float turnBaseTriggerChance; // Probabilitas memicu combat turn-based (0.0 - 1.0)
    bool canTriggerTurnBased;    // Apakah enemy ini eligible memicu combat turn-based
};

/** @brief Ukuran dan offset hitbox enemy, di-load dari JSON */
struct EnemyHitboxData
{
    Vector2 size;   // Lebar dan tinggi hitbox {width, height}
    Vector2 offset; // Offset hitbox relatif terhadap Position {offsetX, offsetY}
};

/** @brief Single source of truth untuk satu tipe enemy, di-load dari JSON */
struct EnemyDefinition
{
    int id;                      ///< ID unik, digunakan sebagai key lookup
    std::string name;            ///< Nama tipe enemy (e.g. "Slime", "Skeleton")
    EnemyStats stats;            ///< Statistik gameplay
    EnemyHitboxData hitbox;      ///< Konfigurasi hitbox
    EnemyRank rank = ENEMY_NORMAL; ///< Rank untuk spawn/balance
    float Scale = 1.0f;           ///< Skala visual (1.0 = normal, 1.25 = elite, 1.75 = boss)
    int potionWeight = 5;          ///< Bobot potion saat roll kategori loot (default 5)
    int weaponWeight = 5;          ///< Bobot weapon saat roll kategori loot (default 5)
    std::string AnimSetName;      ///< Nama AnimationSet yang digunakan (e.g. "Slime", "Skeleton", "Wolf")
    const AnimationSet *animSet;  ///< Pointer ke AnimationSet global, di-resolve dari AnimSetName
};

/** @brief Data-driven manager untuk definisi enemy */
class EnemyDataManager
{
public:
    /** @brief Load definisi enemy dari file JSON */
    void Load(const std::string &path);
    bool Has(const std::string &name) const;
    
    /** @brief Ambil definisi enemy berdasarkan nama */
    const EnemyDefinition &Get(const std::string &name) const;
    /** @brief Ambil semua nama enemy yang sudah di-load */
    std::vector<std::string> GetAllNames() const;

private:
    std::unordered_map<std::string, EnemyDefinition> definitions_; // lookup definisi enemy berdasarkan nama
};

/** @brief Resolve AnimationSet dari nama enemy */
const AnimationSet *ResolveAnimSet(const std::string &name);

/*==============================================================================
 * Enemy Class
 *==============================================================================*/

/**
 * @brief Kelas musuh dengan logika AI berbasis FSM
 *
 * Mewarisi dari Entity untuk properti dasar (Position, Health, dll).
 */
class Enemy : public Entity
{
public:
    /** @brief Constructor */
    Enemy();
    /** @brief Virtual destructor */
    virtual ~Enemy();

    /**
     * @brief Inisialisasi enemy pada posisi tertentu dari EnemyDefinition
     * @param pos Posisi spawn dalam world space
     * @param name Nama instance enemy
     * @param mapId ID object Tiled untuk persistensi kematian
     * @param def Definisi tipe enemy dari EnemyDataManager
     */
    void Init(Vector2 pos, const char *name, int mapId, const EnemyDefinition &def);

    void Update() override;                                             // update state runtime enemy tiap frame
    void Render() override;                                             // gambar enemy, debug overlay, dan health bar bila perlu
    void TakeDamage(float amount, Vector2 knockback = {0, 0}) override; // terima damage dan efek knockback

    void UpdateAI();       // Titik masuk utama logika AI, dipanggil tiap frame
    bool CheckPlayerLoS(); // Cek Line of Sight ke pemain via raycasting
    Rectangle GetAbilityZone() const; // Danger zone rectangle saat elite ability wind-up

    // --- Definisi ---
    const EnemyDefinition *Def = nullptr; // Pointer ke definisi tipe, di-assign saat Init
    EnemyDefinition DefStorage;           // Copy definisi agar pointer tetap valid selama enemy hidup

    // --- Identitas ---
    std::string Name;     // Nama instance enemy
    int MapObjectID = -1; // ID object Tiled untuk persistensi kematian
    EnemyRank rank;       // Rank enemy (normal/elite/boss)

    // --- AI State ---
    EnemyAIState AIState = ENEMY_IDLE; // State FSM aktif saat ini
    float DetectionRange;              // Jarak deteksi aktif, berubah sesuai AIState (runtime)

    // --- Runtime Stats ---
    float HealthRegenTimer; // Countdown sebelum regen aktif, reset saat terkena damage (runtime)

    // --- Turn-Based ---
    bool isTurnBasedMode = false; // True jika sedang dalam mode combat turn-based
    bool isMyTurn = false;        // True jika giliran enemy di mode turn-based


    // --- Animasi ---
    Animation Anim;              // State animasi aktif (runtime)
    const AnimationSet *AnimSet; // Pointer ke AnimationSet aktif, di-resolve dari Def->type

    // --- Patroli ---
    Vector2 PatrolTarget;              // Titik tujuan patroli saat ini (runtime)
    Vector2 SpawnPoint;                // Titik spawn awal, pusat area patroli
    Rectangle SpawnRect;               // Area spawn asal jika dari rectangle spawn
    float PatrolTimer;                 // Timer tunggu di titik patroli (runtime)
    const float PatrolWaitTime = 2.0f; // Durasi tunggu sebelum patroli ke titik berikutnya
    int PatrolFailCount = 0;           // Counter gagal patrol berturut-turut — progressive backoff
    float PatrolStuckTimer = 0;        // Timer deteksi macet di HandlePatrol

    // --- Hitbox ---
    float HitboxWidth;   // Lebar hitbox enemy
    float HitboxHeight;  // Tinggi hitbox enemy
    float HitboxOffsetX; // Offset X hitbox enemy
    float HitboxOffsetY; // Offset Y hitbox enemy
    Rectangle GetHitbox() const override
    {
        return {Position.x + HitboxOffsetX, Position.y + HitboxOffsetY, HitboxWidth, HitboxHeight};
    }

    EnemySteering Steering; // helper steering untuk chase dan return pathfinding

    /** @brief Hitung effective attack range edge-to-edge (attackRange + enemyRadius + playerRadius) */
    float GetEffectiveAttackRange() const;

    /** @brief Ambil velocity enemy dari frame terakhir */
    Vector2 GetVelocity() { return Velocity; }

    // getter/setter untuk AttackCooldownTimer (private)
    float GetAttackCooldownTimer() const { return AttackCooldownTimer; }
    void SetAttackCooldownTimer(float t) { AttackCooldownTimer = t; }

    // cast ray debug dari pusat enemy dengan mode line atau cone
    RayHitResult CastDebugRay(Vector2 dir, float maxDist, std::vector<MapObject> &obstacles,
                              RayCastMode mode, float halfAngleDeg, int rayCount)
    {
        if (mode == RayCastMode::CONE)
            return Ray.CastCone(GetCenter(), dir, maxDist, halfAngleDeg, rayCount, obstacles);
        return Ray.Cast(GetCenter(), dir, maxDist, obstacles);
    }
    /** @brief Ambil panjang raycast steering */
    float GetRayLength() { return rayLength; }
    /** @brief Ambil radius deteksi langsung steering */
    float GetRayDetectionLength() { return rayDetectionLength; }
    /** @brief Ambil ukuran hitbox untuk pathfinding */
    float GetHitboxValue() { return HitBoxValue; }
    /** @brief Ambil offset hitbox untuk pathfinding */
    float GetOffSetValue() { return OffSetValue; }
    /** @brief Ambil offset pusat tile */
    float GetTileCenterOffset() { return TileCenterOffset; }

    const FlowField *GetReturnFlowField() const { return ReturnFlowField; } // ambil flow field return aktif
    void SetReturnFlowField(FlowField *ff) { ReturnFlowField = ff; }        // set flow field untuk kembali ke spawn

    /** @brief Bangun konteks steering dari state runtime enemy dan player saat ini */
    SteeringContext BuildSteeringContext() const
    {
        SteeringContext ctx;
        ctx.Position = Position;
        ctx.Velocity = Velocity;
        ctx.HitBoxValue = HitBoxValue;
        ctx.OffsetValue = OffSetValue;
        ctx.TileCenterOffset = TileCenterOffset;
        ctx.DetectionRange = DetectionRange;
        ctx.rayLength = rayLength;
        ctx.rayDetectionLength = rayDetectionLength;
        ctx.PlayerCenter = PlayerInstance.GetCenter();
        ctx.PlayerHitbox = PlayerInstance.GetHitbox();
        ctx.SpawnPoint = SpawnPoint;
        ctx.ReturnFlowField = ReturnFlowField;
        return ctx;
    }

    Vector2 SeparationForce = {0, 0}; // Gaya separation dari enemy lain

    // --- Feedback Visual & Kematian (runtime, accessible externally) ---
    float HitFlashTimer = 0.0f;       // Timer tint merah saat terkena damage (runtime)
    float HealthBarTimer = 0.0f;      // Timer health bar muncul setelah kena damage (runtime)
    const float HealthBarDuration = 1.5f; // Durasi health bar muncul
    Vector2 KnockbackVelocity;        // Vektor knockback aktif (runtime)
    float DeathTimer = 0.0f;          // Timer animasi kematian (runtime)
    const float DeathDuration = 1.2f; // Durasi animasi kematian sebelum di-deactivate

private:
    void HandleIdle();     // Jalankan state idle
    void HandlePatrol();   // Jalankan state patrol
    void HandleChase();    // Jalankan state chase
    void HandleAttack();   // Jalankan state attack
    void HandleAbility1(); // Jalankan state ability1 (elite special / boss AOE slam)
    void HandleAbility2(); // Jalankan state ability2 (boss charge)
    void HandleReturn();   // Jalankan state return
    void PerformAttack();  // Eksekusi damage ke player

    FlowField *ReturnFlowField = nullptr; // Flow field untuk kembali ke spawn

    Vector2 Velocity = {0, 0};                    // Arah gerak frame sebelumnya (dinormalisasi)
    RayCast Ray;                                  // Pemeriksaan LoS dan deteksi obstacle
    float TileCenterOffset = FRAME_SIZE * 0.5f;   // Offset untuk AI pathfinding
    float HitBoxValue = 24.0f;                    // Hitbox untuk AI pathfinding
    float OffSetValue = 0.0f;                     // Offset untuk AI pathfinding
    float rayLength = FRAME_SIZE * 2.0f;          // Panjang raycast untuk AI pathfinding
    float rayDetectionLength = FRAME_SIZE * 2.1f; // Radius deteksi langsung ke player
    float ReturnScanTimer = 0.f;                  // Timer pencarian ulang return flow field

    float AttackCooldownTimer;         // Sisa waktu cooldown serangan (runtime)
    const float AttackCooldown = 1.0f; // Durasi cooldown antar serangan
    float AttackWindUpTimer = 0.0f;    // Timer wind-up serangan elite sebelum damage (runtime)
    float AbilityTimer = 0.0f;         // Timer periodik ability elite (4-5 detik)
    float BossAbilityTimer = 0.0f;     // Timer periodik ability boss AOE slam (5 detik)
    float BossAbility2Timer = 0.0f;    // Timer periodik ability boss charge (7 detik)
    bool PlayerWasInRange = false;     // Flag mencegah serangan ganda dalam satu frame
    float ChargeDistanceRemaining = 0.0f;     // Sisa jarak charge ability2 (pixel)
    Vector2 ChargeDir = {0, 0};              // Arah charge ability2
    bool ChargeHitPlayer = false;            // Cegah multiple hit per charge

    void MoveTowards(Vector2 target, float speed); // Helper gerak ke target dengan collision check
};

/*==============================================================================
 * Utility Functions
 *==============================================================================*/
/** @brief Parse string rank ke enum */
EnemyRank ParseRank(const std::string &s);
/** @brief Ambil nama enemy sesuai rank */
std::vector<std::string> GetNamesByRank(EnemyRank rank);

/** @brief Spawn enemy dari object spawn di map aktif */
void SpawnEnemiesFromMap();
/** @brief Spawn satu enemy di titik berdasarkan rank */
void SpawnAtPoint(const MapObject *obj, EnemyRank rank);
/** @brief Spawn enemy acak di area rectangle */
void SpawnInRect(const MapObject *obj, const std::string &enemyName, float ratio);
/** @brief Spawn boss */
void SpawnBoss(const MapObject *obj);
/** @brief Load texture dan data enemy */
void InitEnemy();

/** @brief Hapus semua enemy aktif */
void ClearEnemies();

/** @brief Instance global EnemyDataManager */
extern EnemyDataManager enemyData;
