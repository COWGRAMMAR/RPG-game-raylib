/**
 * @file enemy.cpp
 * @brief Implementasi Enemy Entity System
 *
 * File ini berisi implementasi class Enemy:
 * - EnemyDataManager: load/query definisi enemy dari JSON
 * - Enemy lifecycle: Init, Update, AI state machine, Render
 * - Spawn system: SpawnAtPoint, SpawnInRect, SpawnBoss, SpawnEnemiesFromMap
 * - Combat: PerformAttack, TakeDamage
 * - Utility: MoveTowards, ResolveAnimSet, ParseRank
 */

#include "enemy.h"
#include "screen.h"
#include "enemy_ai.h"
#include "player.h"
#include "map.h"
#include "datadriven.h"
#include "../lib/raylib/include/raymath.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include "game_debug.h"
#include "entities.h"
#include "core/utils.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>

using json = nlohmann::json;
using namespace DataDriven;

/** @brief Instance global definisi enemy */
EnemyDataManager enemyData;

/*==============================================================================
 * Enemy Data Utilities
 *==============================================================================*/

/**
 * @brief Konversi string rank dari data JSON menjadi enum EnemyRank.
 * @param s String rank enemy
 * @return Enum EnemyRank, default ENEMY_NORMAL jika string tidak dikenali
 */
EnemyRank ParseRank(const std::string &s)
{
    if (s == "elite")
        return ENEMY_ELITE;
    if (s == "boss")
        return ENEMY_BOSS;
    return ENEMY_NORMAL;
}

/**
 * @brief Ambil semua nama enemy yang memiliki rank tertentu.
 * @param rank Rank enemy yang dicari
 * @return Daftar nama enemy dengan rank yang cocok
 */
std::vector<std::string> GetNamesByRank(EnemyRank rank)
{
    std::vector<std::string> result;
    for (const auto &name : enemyData.GetAllNames())
    {
        if (enemyData.Get(name).rank == rank)
            result.push_back(name);
    }
    return result;
}

/*==============================================================================
 * EnemyDataManager
 *==============================================================================*/

/**
 * @brief Load seluruh definisi enemy dari file JSON.
 * @param path Path file JSON enemy data
 * @note Melempar runtime_error jika file tidak bisa dibuka.
 */
void EnemyDataManager::Load(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open: " + path);

    json root = json::parse(file);

    for (auto &[name, data] : root.at("enemies").items())
    {
        EnemyDefinition def;
        def.id = SafeGet<int>(data, "id", -1);
        def.name = name;
        def.rank = ParseRank(SafeGet<std::string>(data, "rank", "normal"));

        const auto &s = data.at("stats");
        def.stats.maxHealth = SafeGet<float>(s, "maxHealth", 100.f);
        def.stats.speed = SafeGet<float>(s, "speed", 1.f);
        def.stats.chaseSpeed = SafeGet<float>(s, "chaseSpeed", 1.5f);
        def.stats.damage = SafeGet<float>(s, "damage", 10.f);
        def.stats.baseDetectionRange = SafeGet<float>(s, "baseDetectionRange", 120.f);
        def.stats.chaseDetectionRange = SafeGet<float>(s, "chaseDetectionRange", 240.f);
        def.stats.attackRange = SafeGet<float>(s, "attackRange", 32.f);
        def.stats.healthRegenRate = SafeGet<float>(s, "healthRegenRate", 10.f);
        def.stats.healthRegenDelay = SafeGet<float>(s, "healthRegenDelay", 1.f);
        def.stats.patrolRadius = SafeGet<float>(s, "patrolRadius", 130.f);
        def.stats.turnBaseTriggerChance = SafeGet<float>(s, "turnBaseTriggerChance", 0.f);
        def.stats.canTriggerTurnBased = SafeGet<bool>(s, "canTriggerTurnBased", false);

        const auto &h = data.at("hitbox");
        def.hitbox.size = ParseVector2(h.at("size"));
        def.hitbox.offset = ParseVector2(h.at("offset"));

        def.animSet = ResolveAnimSet(name);

        definitions_[name] = std::move(def);
    }
}

/**
 * @brief Ambil definisi enemy berdasarkan nama.
 * @param name Nama enemy yang dicari
 * @return Referensi definisi enemy
 * @note Melempar runtime_error jika nama tidak ditemukan.
 */
const EnemyDefinition &EnemyDataManager::Get(const std::string &name) const
{
    auto it = definitions_.find(name);
    if (it == definitions_.end())
        throw std::runtime_error("EnemyDefinition not found: " + name);
    return it->second;
}

/**
 * @brief Ambil daftar semua nama enemy yang sudah dimuat.
 * @return Daftar nama enemy dari enemy data manager
 */
std::vector<std::string> EnemyDataManager::GetAllNames() const
{
    std::vector<std::string> names;
    names.reserve(definitions_.size());
    for (auto &[k, _] : definitions_)
        names.push_back(k);
    return names;
}

/*==============================================================================
 * Enemy — Lifecycle
 *==============================================================================*/

/** @brief Default constructor */
Enemy::Enemy()
{
    IsActive = true;
}

/** @brief Default destructor */
Enemy::~Enemy() {}

/**
 * @brief Inisialisasi enemy dari definisi yang sudah dimuat.
 * @param pos Posisi spawn di world space
 * @param name Nama enemy (harus cocok dengan key di enemies.json)
 * @param mapId ID object di Tiled, dipakai untuk RegisterDeath
 * @param def Definisi enemy yang sudah dimuat dari EnemyDataManager
 */
void Enemy::Init(Vector2 pos, const char *name, int mapId, const EnemyDefinition &def)
{
    DefStorage = def;
    Def = &DefStorage;
    AnimSet = Def->animSet;
    Anim.animSet = AnimSet;
    MapObjectID = mapId;
    SpawnPoint = pos;
    SpawnRect = {0, 0, 0, 0};
    Name = name;
    rank = def.rank;

    Health = def.stats.maxHealth;
    MaxHealth = def.stats.maxHealth;
    HitboxWidth = def.hitbox.size.x;
    HitboxHeight = def.hitbox.size.y;
    HitboxOffsetX = def.hitbox.offset.x;
    HitboxOffsetY = def.hitbox.offset.y;

    // Runtime state — reset setiap spawn
    DetectionRange = def.stats.baseDetectionRange;
    HealthRegenTimer = 0.0f;
    PatrolTimer = 0.0f;
    AttackCooldownTimer = 0.0f;
    HitFlashTimer = 0.0f;
    KnockbackVelocity = {0, 0};
    DeathTimer = 0.0f;
    PlayerWasInRange = false;
    AIState = ENEMY_IDLE;

    // Posisi disesuaikan agar hitbox center-nya tepat di pos spawn
    Position.x = pos.x - (HitboxWidth / 2.0f) - HitboxOffsetX;
    Position.y = pos.y - (HitboxHeight / 2.0f) - HitboxOffsetY;
    PatrolTarget = pos;

    PlayAnimation(Anim, IDLE, DOWN);
    Anim.position = Position;
}

/**
 * @brief Update lifecycle enemy, termasuk death state, knockback, AI, dan animasi.
 */
void Enemy::Update()
{
    if (!IsActive)
        return;

    if (Health <= 0)
    {
        if (Anim.state != DEAD)
        {
            PlayAnimation(Anim, DEAD, Anim.direction);
            AIState = ENEMY_IDLE;
            DetectionRange = Def->stats.baseDetectionRange;
            Entities::RegisterDeath(GetCurrentMapPath(), MapObjectID);
        }

        DeathTimer += Time::DELTA_TIME;
        if (DeathTimer >= DeathDuration)
            IsActive = false;

        Anim.position = Position;
        UpdateAnimation(Anim, Time::DELTA_TIME);
        return;
    }

    if (HitFlashTimer > 0)
        HitFlashTimer -= Time::DELTA_TIME;
    if (AttackCooldownTimer > 0)
        AttackCooldownTimer -= Time::DELTA_TIME;

    float fpsNorm = 60.0f;
    float knockbackFriction = 0.85f;
    if (Vector2Length(KnockbackVelocity) > 0.1f)
    {
        Vector2 move = Vector2Scale(KnockbackVelocity, Time::DELTA_TIME * fpsNorm);
        Vector2 nextX = {Position.x + move.x, Position.y};
        Vector2 nextY = {Position.x, Position.y + move.y};

        if (IsPositionSafe(nextX, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
            Position.x = nextX.x;
        if (IsPositionSafe(nextY, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
            Position.y = nextY.y;

        KnockbackVelocity = Vector2Scale(KnockbackVelocity, knockbackFriction);
    }
    else
    {
        KnockbackVelocity = {0, 0};
    }

    // buat ngatur sejauh apa ai enemy bisa ke update
    float aiUpdateRangeMul = 20.0f;
    const float AI_UPDATE_RANGE = FRAME_SIZE * aiUpdateRangeMul;

    if (Vector2Distance(Position, PlayerInstance.GetPosition()) <= AI_UPDATE_RANGE)
        UpdateAI();

    Anim.position = Position;
    UpdateAnimation(Anim, Time::DELTA_TIME);
}

/**
 * @brief Update state machine AI enemy berdasarkan kondisi player dan state aktif.
 */
void Enemy::UpdateAI()
{
    // Jika player mati, paksa idle agar enemy tidak terus mengejar posisi terakhir
    if (!PlayerInstance.IsAlive())
    {
        if (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
        {
            AIState = ENEMY_IDLE;
            PlayAnimation(Anim, IDLE, Anim.direction);
        }
        return;
    }

    // Detection range diperluas saat mengejar agar enemy tidak langsung berhenti di tepi range
    DetectionRange = (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
                         ? Def->stats.chaseDetectionRange
                         : Def->stats.baseDetectionRange;

    // Regen HP hanya saat tidak agresif
    if (HealthRegenTimer > 0)
    {
        HealthRegenTimer -= Time::DELTA_TIME;
    }
    else if (AIState != ENEMY_CHASE && AIState != ENEMY_ATTACK && Health < MaxHealth)
    {
        Health += Def->stats.healthRegenRate * Time::DELTA_TIME;
        if (Health > MaxHealth)
            Health = MaxHealth;
    }

    switch (AIState)
    {
    case ENEMY_IDLE:
        HandleIdle();
        break;
    case ENEMY_PATROL:
        HandlePatrol();
        break;
    case ENEMY_CHASE:
        HandleChase();
        break;
    case ENEMY_ATTACK:
        HandleAttack();
        break;
    case ENEMY_RETURN:
        HandleReturn();
        break;
    }
}

/**
 * @brief Cek apakah player dalam jangkauan dan tidak terhalang obstacle.
 * @return True jika player terlihat
 * @note Raycast dilakukan terhadap collision layer Tiled + dynamic obstacles
 */
bool Enemy::CheckPlayerLoS()
{
    if (!tilesonMap || !PlayerInstance.IsAlive())
        return false;

    Vector2 enemyCenter = GetCenter();
    Vector2 playerCenter = PlayerInstance.GetCenter();

    if (!CheckCollisionCircleRec(enemyCenter, DetectionRange, PlayerInstance.GetHitbox()))
        return false;

    Vector2 dir = Vector2Normalize(Vector2Subtract(playerCenter, enemyCenter));

    RayHitResult hit = Ray.Cast(enemyCenter, dir, DetectionRange, cachedObstacleList);
    return !hit.hit;
}

/*==============================================================================
 * Enemy — AI States
 *==============================================================================*/

/**
 * @brief Jalankan state idle enemy.
 */
void Enemy::HandleIdle()
{
    if (CheckPlayerLoS())
    {
        AIState = ENEMY_CHASE;
        return;
    }

    PatrolTimer += Time::DELTA_TIME;
    if (PatrolTimer >= PatrolWaitTime)
    {
        PatrolTimer = 0;
        PatrolTarget = SpawnPoint; // fallback jika tidak ada posisi valid

        // Coba hingga beberapa kali agar patrol target tidak di dalam dinding
        int patrolRetryLimit = 10;
        for (int i = 0; i < patrolRetryLimit; i++)
        {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float r = (float)GetRandomValue(FRAME_SIZE, (int)Def->stats.patrolRadius);
            Vector2 potentialTarget = Vector2Add(SpawnPoint, {cosf(angle) * r, sinf(angle) * r});

            if (IsPositionSafe(potentialTarget, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
            {
                PatrolTarget = potentialTarget;
                break;
            }
        }

        AIState = ENEMY_PATROL;
        PlayAnimation(Anim, WALK, Anim.direction);
    }
}

/**
 * @brief Jalankan state patrol enemy.
 */
void Enemy::HandlePatrol()
{
    if (CheckPlayerLoS())
    {
        AIState = ENEMY_CHASE;
        return;
    }

    float patrolArrivalDist = 10.0f;
    float dist = Vector2Distance(Position, PatrolTarget);
    if (dist < patrolArrivalDist)
    {
        AIState = ENEMY_IDLE;
        PlayAnimation(Anim, IDLE, Anim.direction);
        return;
    }

    MoveTowards(PatrolTarget, Def->stats.speed);
    if (Anim.state != WALK)
        PlayAnimation(Anim, WALK, Anim.direction);
}

/**
 * @brief Jalankan state chase enemy.
 */
void Enemy::HandleChase()
{
    SteeringContext ctx = BuildSteeringContext();

    if (AttackCooldownTimer > 0)
    {
        if (Anim.state != IDLE)
            PlayAnimation(Anim, IDLE, Anim.direction);
        return;
    }

    Vector2 enemyCenter = GetCenter();
    Vector2 playerCenter = PlayerInstance.GetCenter();
    float dist = Vector2Distance(enemyCenter, playerCenter);

    if (dist <= Def->stats.attackRange)
    {
        if (!PlayerWasInRange)
            PerformAttack();
        AIState = ENEMY_ATTACK;
        PlayerWasInRange = true;
        return;
    }

    PlayerWasInRange = false;

    if (!CheckCollisionCircleRec(enemyCenter, DetectionRange, PlayerInstance.GetHitbox()))
    {
        AIState = ENEMY_RETURN;
        PatrolTarget = SpawnPoint;
        PlayAnimation(Anim, WALK, Anim.direction);
        return;
    }

    if (Steering.SteeringFlipCount >= Steering.MaxSteeringFlipCount)
    {
        Steering.SteeringFlipCount = 0;
        AIState = ENEMY_PATROL;
        return;
    }

    if (Steering.IsPlayerInRange(ctx))
    {
        MoveTowards(playerCenter, Def->stats.chaseSpeed);
    }
    else
    {
        Vector2 steerDir = Steering.Compute(STEERING_CHASE, ctx, Ray);
        Velocity = steerDir;

        if (Vector2LengthSqr(steerDir) > 0.001f)
            MoveTowards(Steering.SteeringTarget, Def->stats.chaseSpeed);
        else
            MoveTowards(playerCenter, Def->stats.chaseSpeed);
    }

    if (Anim.state != WALK)
        PlayAnimation(Anim, WALK, Anim.direction);
}

/**
 * @brief Jalankan state return enemy.
 */
void Enemy::HandleReturn()
{
    if (CheckPlayerLoS())
    {
        AIState = ENEMY_CHASE;
        return;
    }

    float returnDistMul = 4.0f;
    bool hasReturned = (SpawnRect.width > 0)
                           ? CheckCollisionPointRec(GetCenter(), SpawnRect)
                           : Vector2Distance(GetCenter(), SpawnPoint) < FRAME_SIZE * returnDistMul;

    if (hasReturned)
    {
        AIState = ENEMY_IDLE;
        PlayAnimation(Anim, IDLE, Anim.direction);
        return;
    }

    // throttle scan spawn flow field
    ReturnScanTimer -= Time::DELTA_TIME;
    if (ReturnScanTimer <= 0.f)
    {
        ReturnScanTimer = RETURN_SCAN_INTERVAL;

        if (ReturnFlowField == nullptr ||
            Vector2LengthSqr(ReturnFlowField->GetDirection(GetCenter())) < 0.001f)
        {
            ReturnFlowField = FindNearestSpawnFlowField(GetCenter());
        }
    }

    SteeringContext ctx = BuildSteeringContext();

    if (Steering.SteeringFlipCount >= Steering.MaxSteeringFlipCount)
    {
        Steering.SteeringFlipCount = 0;
        AIState = ENEMY_IDLE;
        return;
    }

    if (ReturnFlowField == nullptr)
    {
        // fallback — jalan lurus ke spawn point
        MoveTowards(SpawnPoint, Def->stats.speed);
        return;
    }

    Vector2 steerDir = Steering.Compute(STEERING_RETURN, ctx, Ray);
    Velocity = steerDir;

    if (Vector2LengthSqr(steerDir) > 0.001f)
        MoveTowards(Steering.SteeringTarget, Def->stats.speed);
    else
        MoveTowards(SpawnPoint, Def->stats.speed);

    if (Anim.state != WALK)
        PlayAnimation(Anim, WALK, Anim.direction);
}

/**
 * @brief Jalankan state attack enemy.
 */
void Enemy::HandleAttack()
{
    Vector2 enemyCenter = GetCenter();
    Vector2 playerCenter = PlayerInstance.GetCenter();
    float dist = Vector2Distance(enemyCenter, playerCenter);

    if (dist <= Def->stats.attackRange)
    {
        if (!PlayerWasInRange || AttackCooldownTimer <= 0)
            PerformAttack();
        PlayerWasInRange = true;
    }
    else
    {
        PlayerWasInRange = false;
        // Sedikit buffer agar enemy tidak langsung keluar ATTACK state saat player mundur tipis
        float attackExitBuffer = 1.2f;
        if (dist > Def->stats.attackRange * attackExitBuffer)
        {
            AIState = ENEMY_CHASE;
            PlayAnimation(Anim, WALK, Anim.direction);
        }
    }
}

/*==============================================================================
 * Enemy — Combat
 *==============================================================================*/

/**
 * @brief Eksekusi serangan enemy ke player jika tidak terhalang obstacle.
 */
void Enemy::PerformAttack()
{
    Vector2 enemyCenter = GetCenter();
    Vector2 playerCenter = PlayerInstance.GetCenter();

    // cek ada obstacle nggak antara enemy dan player
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerCenter, enemyCenter));
    float dist = Vector2Distance(enemyCenter, playerCenter);
    RayHitResult hit = Ray.Cast(enemyCenter, dir, dist, cachedObstacleList);

    if (hit.hit)
        return; // ada obstacle, batal serang

    Vector2 knockDir = dir;
    PlayerInstance.TakeDamage(Def->stats.damage, knockDir);

    PlayAnimation(Anim, ATTACK, Anim.direction);
    Anim.isAttacking = true;
    AttackCooldownTimer = AttackCooldown;
}

/**
 * @brief Enemy menerima damage dan knockback.
 * @param amount Jumlah damage
 * @param knockback Arah knockback (normalized)
 * @note HealthRegenTimer di-reset agar regen tidak langsung jalan setelah kena hit
 */
void Enemy::TakeDamage(float amount, Vector2 knockback)
{
    Entity::TakeDamage(amount, knockback);
    float hitFlashDuration = 0.15f;
    float knockbackStrength = 6.0f;
    HitFlashTimer = hitFlashDuration;
    KnockbackVelocity = Vector2Scale(knockback, knockbackStrength);
    HealthRegenTimer = Def->stats.healthRegenDelay;
}

/*==============================================================================
 * Enemy — Render
 *==============================================================================*/

/**
 * @brief Render enemy, efek visual, health bar, dan debug overlay.
 */
void Enemy::Render()
{
    if (!IsActive)
        return;

    // Shadow sederhana di bawah enemy
    int shadowRx = 10, shadowRy = 4;
    DrawEllipse((int)Position.x + FRAME_SIZE / 2, (int)Position.y + FRAME_SIZE - 2, shadowRx, shadowRy, {0, 0, 0, 80});

    bool shouldDraw = true;
    if (Health <= 0)
    {
        // Blink makin cepat menjelang akhir death timer
        float blinkSpeedMul = 15.0f;
        float blinkFreq = (DeathTimer / DeathDuration) * blinkSpeedMul;
        shouldDraw = Blink(DeathTimer, blinkFreq);
    }

    if (shouldDraw)
    {
        Color tint = (HitFlashTimer > 0) ? RED : WHITE;
        DrawAnimation(Anim, tint);
    }

    // Health bar hanya tampil saat agresif
    if (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
    {
        int hpBarX = 4, hpBarY = 38, hpBarW = 24, hpBarH = 4;
        DrawRectangle((int)Position.x + hpBarX, (int)Position.y + hpBarY, hpBarW, hpBarH, BLACK);
        DrawRectangle((int)Position.x + hpBarX, (int)Position.y + hpBarY, (int)(hpBarW * (Health / MaxHealth)), hpBarH, RED);
    }

    if (isDebugMode)
    {
        Vector2 enemyCenter = GetCenter();
        DrawCircleLinesV(enemyCenter, DetectionRange, Fade(GRAY, 0.6f));
        DrawCircleLinesV(enemyCenter, Def->stats.attackRange, RED);
        DrawRectangleLinesEx(GetHitbox(), 1.0f, VIOLET);

        if (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK)
            DrawLineEx(enemyCenter, PlayerInstance.GetCenter(), 1.0f, RED);

        // Steering debug
        if (AIState == ENEMY_CHASE || AIState == ENEMY_RETURN)
            Debug::DrawSteeringOverlay(*this);
    }
}

/*==============================================================================
 * Enemy — Global Utilities
 *==============================================================================*/

/**
 * @brief Muat texture dan data definisi enemy.
 */
void InitEnemy()
{
    enemyData.Load("assets/data/enemies.json");
}

/**
 * @brief Save all active enemy states for a map to the saves/enemies/ filesystem directory.
 * @details Serializes each live enemy's position, name, HP, AI state, patrol data, spawn point,
 *          health regen timer, attack cooldown timer, and UUID to a JSON file. Uses atomic
 *          write via .tmp + rename to prevent corruption. Skips inactive enemies.
 *          Save file path derived by sanitizing mapPath (replacing / and \\ with _).
 * @param mapPath Raw map file path used to derive save file name (e.g., "assets/maps/tutorial.json")
 */
void SaveEnemiesForMap(const std::string &mapPath)
{
    // Build per-map save file path
    std::string safeName = mapPath;
    for (auto &c : safeName)
    {
        if (c == '/' || c == '\\') c = '_';
    }
    std::string dir = "saves/enemies";
    std::string filePath = dir + "/" + safeName;

    std::filesystem::create_directories(dir);

    json root;
    json enemiesJson = json::array();

    auto &enemyReg = Entities::GetEnemyRegistry();
    for (const auto &enemy : enemyReg)

    {
        if (!enemy->IsActive) continue;
        json e;
        e["position"] = {enemy->Position.x, enemy->Position.y};
        e["enemyName"] = enemy->Name;
        e["currentHP"] = (int)enemy->Health;
        e["isAlive"] = enemy->IsAlive();
        e["maxHealth"] = enemy->MaxHealth;
        e["aiState"] = (int)enemy->AIState;
        e["patrolTargetX"] = enemy->PatrolTarget.x;
        e["patrolTargetY"] = enemy->PatrolTarget.y;
        e["patrolTimer"] = enemy->PatrolTimer;
        e["mapObjectID"] = enemy->MapObjectID;
        e["spawnPoint"] = {{"x", enemy->SpawnPoint.x}, {"y", enemy->SpawnPoint.y}};
        e["healthRegenTimer"] = enemy->HealthRegenTimer;
        e["attackCooldownTimer"] = enemy->GetAttackCooldownTimer();
        e["uuid"] = enemy->GetUUID();
        enemiesJson.push_back(e);
    }

    root["enemies"] = enemiesJson;

    // Atomic write
    std::string tmpPath = filePath + ".tmp";
    std::ofstream file(tmpPath);
    file << root.dump(4);
    file.close();
    std::filesystem::rename(tmpPath, filePath);
}

/**
 * @brief Load enemy states for a map from the saves/enemies/ filesystem directory.
 * @details Reads the per-map save file, deserializes each enemy's state, and restores
 *          position, HP, AI state, patrol data, spawn point, timers, and UUID to the
 *          matching Enemy instance (matched by MapObjectID + Name). Dead enemies are
 *          registered via Entities::RegisterDeath to prevent respawn.
 * @param mapPath Raw map file path used to derive save file name
 * @return true if at least one enemy was restored, false if no save file or parse failed
 */
bool LoadEnemiesForMap(const std::string &mapPath)
{
    // Build per-map save file path
    std::string safeName = mapPath;
    for (auto &c : safeName)
    {
        if (c == '/' || c == '\\') c = '_';
    }
    std::string filePath = "saves/enemies/" + safeName;

    if (!std::filesystem::exists(filePath))
        return false;

    try
    {
        std::ifstream file(filePath);
        json root = json::parse(file);

        if (!root.contains("enemies"))
            return false;

        auto &enemyReg = Entities::GetEnemyRegistry();
        std::unordered_set<Enemy*> matchedEnemies;
        bool anyRestored = false;

        for (const auto &e : root.at("enemies"))
        {
            int savedMapObjectID = e.value("mapObjectID", -1);
            bool isAlive = e.value("isAlive", true);

            if (!isAlive)
            {
                if (savedMapObjectID >= 0)
                    Entities::RegisterDeath(mapPath, savedMapObjectID);
                continue;
            }

            // Find matching enemy by MapObjectID and restore state
            for (auto &enemy : enemyReg)
            {
                if (enemy == nullptr || matchedEnemies.count(enemy)) continue;
                if (enemy->MapObjectID == savedMapObjectID && enemy->Name == e.value("enemyName", ""))
                {
                    enemy->Position.x = e.at("position")[0].get<float>();
                    enemy->Position.y = e.at("position")[1].get<float>();
                    enemy->Health = e.value("currentHP", 100);
                    enemy->MaxHealth = e.value("maxHealth", 100.0f);
                    enemy->AIState = (EnemyAIState)e.value("aiState", 0);
                    enemy->PatrolTarget.x = e.value("patrolTargetX", 0.0f);
                    enemy->PatrolTarget.y = e.value("patrolTargetY", 0.0f);
                    enemy->PatrolTimer = e.value("patrolTimer", 0.0f);
                    if (e.contains("spawnPoint"))
                    {
                        enemy->SpawnPoint.x = e["spawnPoint"]["x"].get<float>();
                        enemy->SpawnPoint.y = e["spawnPoint"]["y"].get<float>();
                    }
                    enemy->HealthRegenTimer = e.value("healthRegenTimer", 0.0f);
                    enemy->SetAttackCooldownTimer(e.value("attackCooldownTimer", 0.0f));
                    std::string uuid = e.value("uuid", "");
                    if (!uuid.empty())
                        enemy->SetUUID(uuid);
                    enemy->IsActive = true;
                    matchedEnemies.insert(enemy);
                    anyRestored = true;
                    break;
                }
            }
        }

        return anyRestored;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * @brief Hapus semua enemy aktif dari entity manager.
 */

void ClearEnemies()
{
    Entities::Clear();
}

/*==============================================================================
 * Enemy — Helper
 *==============================================================================*/

/**
 * @brief Gerakkan enemy menuju target dengan collision check per axis.
 * @param target Posisi tujuan dalam world space
 * @param speed Kecepatan gerak enemy
 * @note Collision dicek terpisah per axis agar enemy bisa slide di sepanjang dinding.
 */
void Enemy::MoveTowards(Vector2 target, float speed)
{
    Vector2 dir = Vector2Normalize(Vector2Subtract(target, Position));
    Vector2 move = Vector2Scale(dir, speed);

    if (IsPositionSafe({Position.x + move.x, Position.y}, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
        Position.x += move.x;
    if (IsPositionSafe({Position.x, Position.y + move.y}, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
        Position.y += move.y;

    if (std::abs(dir.x) > std::abs(dir.y))
        Anim.direction = (dir.x > 0) ? RIGHT : LEFT;
    else
        Anim.direction = (dir.y > 0) ? DOWN : UP;
}

/**
 * @brief Pilih AnimationSet berdasarkan nama enemy.
 * @param name Nama enemy
 * @return Pointer ke AnimationSet yang sesuai, atau SlimeAnimationSet sebagai fallback
 */
const AnimationSet *ResolveAnimSet(const std::string &name)
{
    std::string lowerName = name;
    for (auto &c : lowerName)
        c = std::tolower(c);

    auto it = loadedAnimationSets.find(lowerName);
    if (it != loadedAnimationSets.end())
    {
        return &it->second;
    }

    it = loadedAnimationSets.find("slime");
    if (it != loadedAnimationSets.end())
    {
        return &it->second;
    }
    return nullptr;
}

/*==============================================================================
 * Enemy — spawn
 *==============================================================================*/

/**
 * @brief Spawn satu enemy di posisi object spawn berdasarkan rank.
 * @param obj Object spawn dari Tiled
 * @param rank Rank enemy yang akan dipilih dari pool
 * @note Pemilihan enemy deterministic berdasarkan ID object spawn.
 */
void SpawnAtPoint(const MapObject *obj, EnemyRank rank)
{
    if (!obj)
        return;

    auto pool = GetNamesByRank(rank);
    if (pool.empty())
        return;

    std::mt19937 rng(obj->id);
    std::uniform_int_distribution<int> pickDist(0, (int)pool.size() - 1);
    std::uniform_int_distribution<int> countDist(
        rank == ENEMY_ELITE ? SPAWN_PINPOINT_ELITE_MIN : SPAWN_PINPOINT_NORMAL_MIN,
        rank == ENEMY_ELITE ? SPAWN_PINPOINT_ELITE_MAX : SPAWN_PINPOINT_NORMAL_MAX);
    std::uniform_real_distribution<float> offsetDist(-SEPARATION_RADIUS, SEPARATION_RADIUS);

    int count = countDist(rng);

    Vector2 center = {obj->bounds.x + obj->bounds.width / 2.0f,
                      obj->bounds.y + obj->bounds.height / 2.0f};
    if (spawnFlowFields.find(obj->id) == spawnFlowFields.end())
        BuildSpawnFlowFields(center, obj->id, tilesonMap->width, tilesonMap->height);

    for (int i = 0; i < count; i++)
    {
        std::string picked = pool[pickDist(rng)];
        const EnemyDefinition &def = enemyData.Get(picked);

        Vector2 spawnPos = {center.x + offsetDist(rng), center.y + offsetDist(rng)};

        Enemy *enemy = new Enemy();
        enemy->Init(spawnPos, picked.c_str(), obj->id, def);
        enemy->SetUUID(GenerateUUID());
        enemy->SetReturnFlowField(&spawnFlowFields[obj->id].field);
        Entities::AddDynamic(enemy);
    }
}

/**
 * @brief Spawn sejumlah enemy acak di dalam rectangle spawn.
 * @param obj Object rectangle spawn dari Tiled
 * @param enemyName Nama enemy yang akan di-spawn
 * @param ratio Pengali jumlah spawn hasil random
 * @note Posisi spawn dicoba ulang sampai SPAWN_RETRY_LIMIT agar tidak masuk obstacle.
 */
void SpawnInRect(const MapObject *obj, const std::string &enemyName, float ratio)
{
    if (!obj)
        return;

    const EnemyDefinition &def = enemyData.Get(enemyName);

    std::mt19937 rng(obj->id);
    std::uniform_int_distribution<int> maxDist(
        def.rank == ENEMY_ELITE ? SPAWN_RECT_ELITE_MIN : SPAWN_RECT_NORMAL_MIN,
        def.rank == ENEMY_ELITE ? SPAWN_RECT_ELITE_MAX : SPAWN_RECT_NORMAL_MAX);
    std::uniform_real_distribution<float> xDist(obj->bounds.x, obj->bounds.x + obj->bounds.width);
    std::uniform_real_distribution<float> yDist(obj->bounds.y, obj->bounds.y + obj->bounds.height);

    int count = (int)std::round(maxDist(rng) * ratio);

    Vector2 rectCenter = {obj->bounds.x + obj->bounds.width / 2.0f,
                          obj->bounds.y + obj->bounds.height / 2.0f};
    if (spawnFlowFields.find(obj->id) == spawnFlowFields.end())
        BuildSpawnFlowFields(rectCenter, obj->id, tilesonMap->width, tilesonMap->height);

    for (int i = 0; i < count; i++)
    {
        Vector2 spawnPos;
        bool valid = false;

        for (int retry = 0; retry < SPAWN_RETRY_LIMIT; retry++)
        {
            spawnPos = {xDist(rng), yDist(rng)};
            // Convert center (Enemy::Init expectation) ke Entity::Position (IsPositionSafe expectation)
            Vector2 entityPos = {spawnPos.x - def.hitbox.size.x / 2.0f - def.hitbox.offset.x,
                                 spawnPos.y - def.hitbox.size.y / 2.0f - def.hitbox.offset.y};
            if (IsPositionSafe(entityPos, def.hitbox.size.x, def.hitbox.size.y,
                               def.hitbox.offset.x, def.hitbox.offset.y))
            {
                valid = true;
                break;
            }
        }

        if (!valid)
            continue;

        Enemy *enemy = new Enemy();
        enemy->Init(spawnPos, enemyName.c_str(), obj->id, def);
        enemy->SetUUID(GenerateUUID());
        enemy->SpawnRect = obj->bounds;
        enemy->SetReturnFlowField(&spawnFlowFields[obj->id].field);
        Entities::AddDynamic(enemy);
    }
}

/**
 * @brief Spawn satu boss dari object spawn.
 * @param obj Object spawn boss dari Tiled
 * @note Pemilihan boss deterministic berdasarkan ID object spawn.
 */
void SpawnBoss(const MapObject *obj)
{
    if (!obj)
        return;

    auto pool = GetNamesByRank(ENEMY_BOSS);
    if (pool.empty())
        return;

    std::mt19937 rng(obj->id);
    std::uniform_int_distribution<int> pickDist(0, (int)pool.size() - 1);

    std::string picked = pool[pickDist(rng)];
    const EnemyDefinition &def = enemyData.Get(picked);

    Vector2 spawnPos = {obj->bounds.x + obj->bounds.width / 2.0f,
                        obj->bounds.y + obj->bounds.height / 2.0f};

    if (spawnFlowFields.find(obj->id) == spawnFlowFields.end())
        BuildSpawnFlowFields(spawnPos, obj->id, tilesonMap->width, tilesonMap->height);

    Enemy *enemy = new Enemy();
    enemy->Init(spawnPos, picked.c_str(), obj->id, def);
    enemy->SetUUID(GenerateUUID());
    enemy->SetReturnFlowField(&spawnFlowFields[obj->id].field);
    Entities::AddDynamic(enemy);
}

/**
 * @brief Spawn semua enemy dari object spawn di map aktif.
 * @note Object yang sudah tercatat mati untuk map saat ini akan dilewati.
 */
void SpawnEnemiesFromMap()
{
    if (!tilesonMap)
        return;

    auto spawnObjects = TiledHelper::GetObjectsByType("spawn");
    if (spawnObjects.empty())
        return;

    for (const auto *obj : spawnObjects)
    {
        std::mt19937 rng;
        if (Entities::IsAlreadyDead(GetCurrentMapPath(), obj->id))
            continue;

        rng.seed(obj->id);
        std::uniform_real_distribution<float> ratioDist(0.0f, 1.0f);

        if (obj->name == ENEMY_SPAWN_NORMAL_PIN_OBJECT_NAME)
        {
            SpawnAtPoint(obj, ENEMY_NORMAL);
        }
        else if (obj->name == ENEMY_SPAWN_ELITE_PIN_OBJECT_NAME)
        {
            SpawnAtPoint(obj, ENEMY_ELITE);
        }
        else if (obj->name == ENEMY_SPAWN_NORMAL_REC_OBJECT_NAME)
        {
            auto pool = GetNamesByRank(ENEMY_NORMAL);
            for (const auto &name : pool)
                SpawnInRect(obj, name, ratioDist(rng));
        }
        else if (obj->name == ENEMY_SPAWN_ELITE_REC_OBJECT_NAME)
        {
            auto pool = GetNamesByRank(ENEMY_ELITE);
            for (const auto &name : pool)
                SpawnInRect(obj, name, ratioDist(rng));
        }
        else if (obj->name == ENEMY_SPAWN_BOSS_OBJECT_NAME)
        {
            SpawnBoss(obj);
        }
    }
}
