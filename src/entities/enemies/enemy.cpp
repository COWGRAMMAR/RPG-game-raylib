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
#include "systems/audioManager.h"
#include "screen.h"
#include "enemy_ai.h"
#include "combatTurn.h"
#include "player.h"
#include "map.h"
#include "datadriven.h"
#include "propsbehavior.h"
#include "raymath.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include "game_debug.h"
#include "entities.h"
#include "animation.h"
#include "item.h"
#include "core/utils.h"
#include "core/seedmanager.h"
#include "audioManager.h"
#include <map>
#include <random>
#include "core/game_state_saver.h"
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

        if (data.contains("hurtbox"))
        {
            const auto &hu = data.at("hurtbox");
            def.hurtbox.size = ParseVector2(hu.at("size"));
            def.hurtbox.offset = ParseVector2(hu.at("offset"));
        }
        else
        {
            def.hurtbox.size = {32.0f, 32.0f};
            def.hurtbox.offset = {0.0f, 0.0f};
        }

        def.Scale = SafeGet<float>(data, "scale", 1.0f);
        def.potionWeight = SafeGet<int>(data, "potionWeight", 5);
        def.weaponWeight = SafeGet<int>(data, "weaponWeight", 5);
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
    PatrolFailCount = 0;
    PatrolStuckTimer = 0;
    AttackCooldownTimer = 0.0f;
    AttackWindUpTimer = 0.0f;
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
        isTurnBasedMode = false; // Prevent re-trigger turn-based jika mati kena bomb dll
        HealthBarTimer = 0.0f; // Langsung matikan health bar sebelum death anim

        if (Anim.state != DEAD)
        {
            PlayAnimation(Anim, DEAD, Anim.direction);
            AIState = ENEMY_IDLE;
            DetectionRange = Def->stats.baseDetectionRange;
            Entities::RegisterDeathByUUID(GetCurrentMapPath(), GetUUID());

            if (!Def->stats.canTriggerTurnBased)
            {
                // Loot pipeline: dropChance → rollCategory → rollRarity → spawn
                static std::mt19937 lootRng(std::random_device{}());

                // Drop chance per rank
                float dropChance = (rank == ENEMY_ELITE) ? LOOT_DROP_CHANCE_ELITE : LOOT_DROP_CHANCE_NORMAL;
                std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
                bool shouldDrop = chanceDist(lootRng) <= dropChance;

                if (shouldDrop)
                {
                    // Roll kategori (potion vs weapon)
                    int totalWeight = Def->potionWeight + Def->weaponWeight;
                    if (totalWeight <= 0)
                        totalWeight = 1;
                    std::uniform_int_distribution<int> catDist(0, totalWeight - 1);
                    ItemCategory category = (catDist(lootRng) < Def->potionWeight) ? ITEM_POTION : ITEM_WEAPON;

                    // Roll rarity per rank
                    static const std::map<ItemRarity, int> normalRarity = {{RARITY_COMMON, LOOT_RARITY_COMMON}, {RARITY_UNCOMMON, LOOT_RARITY_UNCOMMON}};
                    static const std::map<ItemRarity, int> eliteRarity = {{RARITY_UNCOMMON, LOOT_RARITY_ELITE_UNCOMMON}, {RARITY_RARE, LOOT_RARITY_ELITE_RARE}};
                    const auto &rarityWeights = (rank == ENEMY_ELITE) ? eliteRarity : normalRarity;

                    Vector2 spawnPos = Position;
                    Vector2 itemHS = itemDefs.GetMaxHitboxForCategory(category);
                    float halfW = itemHS.x * 0.5f;
                    float halfH = itemHS.y * 0.5f;
                    float spread = 40.0f;
                    for (int retry = 0; retry < 15; retry++)
                    {
                        Vector2 candidate = {
                            Position.x + (float)GetRandomValue(-(int)spread, (int)spread),
                            Position.y + (float)GetRandomValue(-(int)spread, (int)spread)};
                        Vector2 topLeft = {candidate.x - halfW, candidate.y - halfH};
                        if (IsPositionSafe(topLeft, itemHS.x, itemHS.y, 0, 0))
                        {
                            spawnPos = candidate;
                            break;
                        }
                    }
                    itemData.SpawnItemAtLocation(spawnPos, rarityWeights, category);
                }
            }
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
    if (HealthBarTimer > 0)
        HealthBarTimer -= Time::DELTA_TIME;
    if (AttackCooldownTimer > 0)
        AttackCooldownTimer -= Time::DELTA_TIME;
    if (AbilityTimer > 0)
        AbilityTimer -= Time::DELTA_TIME;
    if (BossAbilityTimer > 0)
        BossAbilityTimer -= Time::DELTA_TIME;
    if (BossAbility2Timer > 0)
        BossAbility2Timer -= Time::DELTA_TIME;

    // Freeze enemy selama turn-based combat — AI & movement dijeda
    if (TurnCombat::IsActive())
    {
        Anim.position = Position;
        UpdateAnimation(Anim, Time::DELTA_TIME);
        return;
    }

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
    float aiUpdateRangeMul = 200.0f;
    const float AI_UPDATE_RANGE = FRAME_SIZE * aiUpdateRangeMul;

    if (Vector2Distance(Position, PlayerInstance.GetPosition()) <= AI_UPDATE_RANGE)
        UpdateAI();

    Anim.position = Position;
    UpdateAnimation(Anim, Time::DELTA_TIME);

    // Elite: prevent attack animation auto-transition to IDLE during wind-up
    if (rank == ENEMY_ELITE && AttackWindUpTimer > 0 && Anim.state == ATTACK && Anim.currentConfig)
        Anim.timer = fminf(Anim.timer, Anim.currentConfig->speed - 0.001f);
}

/**
 * @brief Update state machine AI enemy berdasarkan kondisi player dan state aktif.
 */
void Enemy::UpdateAI()
{
    // Turn-based trigger: boss dengan HP ≤ 50% memicu combat turn-based
    // Catatan: boss music ambient di-handle oleh UpdateBossMusic() di hud.cpp
    if (Def->stats.canTriggerTurnBased)
    {
        isTurnBasedMode = (Health <= MaxHealth * 0.5f);
    }

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
    DetectionRange = (AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK || AIState == ENEMY_ABILITY1 || AIState == ENEMY_ABILITY2)
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

    // Boss ability triggers (hanya saat player dalam range)
    if (rank == ENEMY_BOSS && AIState != ENEMY_ABILITY1 && AIState != ENEMY_ABILITY2 && AIState != ENEMY_ATTACK)
    {
        float range = fmaxf(Def->stats.chaseDetectionRange, FRAME_SIZE * 2.0f);
        float dist = Vector2Distance(GetCenter(), PlayerInstance.GetCenter());
        if (dist <= range)
        {
            if (BossAbilityTimer <= 0)
            {
                AIState = ENEMY_ABILITY1;
                BossAbilityTimer = 5.0f;
            }
            else if (BossAbility2Timer <= 0)
            {
                AIState = ENEMY_ABILITY2;
                BossAbility2Timer = 7.0f;
            }
        }
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
    case ENEMY_ABILITY1:
        HandleAbility1();
        break;
    case ENEMY_ABILITY2:
        HandleAbility2();
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

    float maxDistPatrol = 6.0f * FRAME_SIZE;
    bool tooFarFromSpawn = (SpawnRect.width > 0)
        ? !CheckCollisionPointRec(GetCenter(), SpawnRect)
        : Vector2Distance(GetCenter(), SpawnPoint) > maxDistPatrol;

    if (tooFarFromSpawn)
    {
        AIState = ENEMY_RETURN;
        PatrolTarget = SpawnPoint;
        PlayAnimation(Anim, WALK, Anim.direction);
        return;
    }

    PatrolTimer += Time::DELTA_TIME;

    // Progressive backoff: makin sering gagal nyari target, makin lama jedanya
    float effectiveWait = PatrolWaitTime;
    if (PatrolFailCount >= 2)
        effectiveWait *= (float)PatrolFailCount;

    if (PatrolTimer >= effectiveWait)
    {
        PatrolTimer = 0;
        PatrolTarget = Position; // fallback: diam di tempat, coba lagi nanti

        // Cari target patrol relatif dari posisi saat ini (bukan SpawnPoint)
        constexpr int patrolRetryLimit = 10;
        for (int i = 0; i < patrolRetryLimit; i++)
        {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float r = (float)GetRandomValue(FRAME_SIZE, (int)Def->stats.patrolRadius);
            Vector2 potentialTarget = Vector2Add(Position, {cosf(angle) * r, sinf(angle) * r});

            if (IsPositionSafe(potentialTarget, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
            {
                PatrolTarget = potentialTarget;
                break;
            }
        }

        if (PatrolTarget == Position)
        {
            constexpr int smallRetryLimit = 5;
            constexpr float smallRadiusMin = 32.0f;
            constexpr float smallRadiusMax = 64.0f;
            for (int i = 0; i < smallRetryLimit; i++)
            {
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float r = (float)GetRandomValue((int)smallRadiusMin, (int)smallRadiusMax);
                Vector2 potentialTarget = Vector2Add(Position, {cosf(angle) * r, sinf(angle) * r});

                if (IsPositionSafe(potentialTarget, HitboxWidth, HitboxHeight, HitboxOffsetX, HitboxOffsetY))
                {
                    PatrolTarget = potentialTarget;
                    break;
                }
            }
        }

        // Update fail counter: reset kalo dapet target, increment kalo gagal total
        if (PatrolTarget == Position)
            PatrolFailCount++;
        else if (PatrolFailCount > 0)
            PatrolFailCount = 0;

        PatrolStuckTimer = 0;
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
        PatrolStuckTimer = 0;
        AIState = ENEMY_IDLE;
        PlayAnimation(Anim, IDLE, Anim.direction);
        return;
    }

    // Abandon kalo stuck terlalu lama (target terhalang obstacle)
    constexpr float PATROL_STUCK_TIMEOUT = 3.0f;
    PatrolStuckTimer += Time::DELTA_TIME;
    if (PatrolStuckTimer >= PATROL_STUCK_TIMEOUT)
    {
        PatrolStuckTimer = 0;
        AIState = ENEMY_IDLE;
        PlayAnimation(Anim, IDLE, Anim.direction);
        return;
    }

    MoveTowards(PatrolTarget, Def->stats.speed);
    if (Anim.state != WALK)
        PlayAnimation(Anim, WALK, Anim.direction);
}

float Enemy::GetEffectiveAttackRange() const
{
    Rectangle playerHb = PlayerInstance.GetHitbox();
    float playerRadius = (playerHb.width + playerHb.height) / 4.0f;
    float enemyRadius = (HitboxWidth + HitboxHeight) / 4.0f;
    return Def->stats.attackRange + enemyRadius + playerRadius;
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

    if (dist <= GetEffectiveAttackRange())
    {
        // Elite: chance ability setiap 4-5 detik
        if (rank == ENEMY_ELITE && AbilityTimer <= 0 && GetRandomValue(0, 99) < 50)
        {
            AbilityTimer = (float)GetRandomValue(4, 5);
            AIState = ENEMY_ABILITY1;
            PlayerWasInRange = true;
            return;
        }

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
 * @brief Jalankan state ability1 elite — wind-up 0.7s lalu 2.5x damage.
 */
void Enemy::HandleAbility1()
{
    float windupDuration = (rank == ENEMY_BOSS) ? 1.0f : 0.7f;

    if (Anim.state != ABILITY1)
    {
        PlayAnimation(Anim, ABILITY1, Anim.direction);
        AttackWindUpTimer = windupDuration;
        Anim.isAttacking = true;
    }

    if (AttackWindUpTimer > 0)
    {
        AttackWindUpTimer -= Time::DELTA_TIME;
        if (Anim.currentConfig && Anim.timer >= Anim.currentConfig->speed)
            Anim.timer = Anim.currentConfig->speed - 0.001f;
        return;
    }

    if (rank == ENEMY_BOSS)
    {
        // Boss AOE slam selesai
        Vector2 ctr = GetCenter();
        float radius = 190.0f;
        if (CheckCollisionCircleRec(ctr, radius, PlayerInstance.GetHitbox()))
        {
            PlayerInstance.TakeDamage(40.0f, Vector2Normalize(Vector2Subtract(PlayerInstance.GetCenter(), ctr)));
        }
    }
    else
    {
        // Elite: damage dalam danger zone
        Rectangle zone = GetAbilityZone();
        if (CheckCollisionPointRec(PlayerInstance.GetCenter(), zone))
        {
            Vector2 dir = Vector2Normalize(Vector2Subtract(PlayerInstance.GetCenter(), GetCenter()));
            float abilityDamage = Def->stats.damage * 2.5f;
            PlayerInstance.TakeDamage(abilityDamage, dir);
        }
        AttackCooldownTimer = AttackCooldown;
    }

    AIState = ENEMY_ATTACK;
}

void Enemy::HandleAbility2()
{
    if (Anim.state != ABILITY2)
    {
        PlayAnimation(Anim, ABILITY2, Anim.direction);
        AttackWindUpTimer = 0.8f;
        Anim.isAttacking = true;
    }

    if (AttackWindUpTimer > 0)
    {
        AttackWindUpTimer -= Time::DELTA_TIME;
        if (Anim.currentConfig && Anim.timer >= Anim.currentConfig->speed)
            Anim.timer = Anim.currentConfig->speed - 0.001f;
        return;
    }

    // Wind-up selesai: inisialisasi charge direction & distance
    if (ChargeDistanceRemaining <= 0)
    {
        if (Anim.direction == LEFT)  ChargeDir = {-1, 0};
        if (Anim.direction == RIGHT) ChargeDir = {1, 0};
        if (Anim.direction == UP)    ChargeDir = {0, -1};
        if (Anim.direction == DOWN)  ChargeDir = {0, 1};
        ChargeDistanceRemaining = 200.0f;
        ChargeHitPlayer = false;
    }

    // Charge gradual tiap frame (skip DynamicObstacles agar bomb tidak menghalangi)
    float step = 4.0f;
    Vector2 next = {Position.x + ChargeDir.x * step, Position.y + ChargeDir.y * step};
    bool blocked = true;
    if (tilesonMap)
    {
        Rectangle hitbox = BuildHitbox(next, HitboxOffsetX, HitboxOffsetY, HitboxWidth, HitboxHeight);
        float worldW = (float)tilesonMap->width * FRAME_SIZE;
        float worldH = (float)tilesonMap->height * FRAME_SIZE;
        blocked = !IsWithinWorldBounds(hitbox, worldW, worldH) ||
                   CheckCollisionAgainstRects(hitbox, gCollisionCache.rects) ||
                   CheckCollisionAgainstPolygons(hitbox, gCollisionCache.polygons);
    }
    if (blocked)
    {
        ChargeDistanceRemaining = 0;
    }
    else
    {
        Position = next;
        ChargeDistanceRemaining -= step;

        // Collision dengan bom saat charge
        Rectangle bossHitbox = {next.x + HitboxOffsetX, next.y + HitboxOffsetY, HitboxWidth, HitboxHeight};
        bombManager.HitByAttack(bossHitbox, PlayerInstance.GetHitbox(), &PlayerInstance);

        if (!ChargeHitPlayer && CheckCollisionRecs(bossHitbox, PlayerInstance.GetHitbox()))
        {
            ChargeHitPlayer = true;
            Vector2 kb = Vector2Scale(ChargeDir, 4.0f);
            PlayerInstance.TakeDamage(50.0f, kb);
        }
    }

    if (ChargeDistanceRemaining <= 0)
    {
        ChargeDistanceRemaining = 0;
        AIState = ENEMY_CHASE;
    }
}

Rectangle Enemy::GetAbilityZone() const
{
    Vector2 center = GetCenter();
    float zoneW = 44.0f, zoneH = 44.0f;
    float zx = center.x - zoneW / 2.0f;
    float zy = center.y - zoneH / 2.0f;
    float offset = 22.0f;
    if (Anim.direction == LEFT)  zx -= offset;
    if (Anim.direction == RIGHT) zx += offset;
    if (Anim.direction == UP)    zy -= offset;
    if (Anim.direction == DOWN)  zy += offset;
    return {zx, zy, zoneW, zoneH};
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
    // If we are winding up an attack, count down and deliver damage when ready
    if (AttackWindUpTimer > 0)
    {
        AttackWindUpTimer -= Time::DELTA_TIME;
        // Prevent auto-transition to IDLE by keeping timer below threshold
        if (rank == ENEMY_ELITE && Anim.currentConfig && Anim.timer >= Anim.currentConfig->speed)
            Anim.timer = Anim.currentConfig->speed - 0.001f;
        if (AttackWindUpTimer <= 0)
        {
            AttackWindUpTimer = 0;
            Vector2 dir = Vector2Normalize(Vector2Subtract(PlayerInstance.GetCenter(), GetCenter()));
            PlayerInstance.TakeDamage(Def->stats.damage, dir);
            AttackCooldownTimer = AttackCooldown;
        }
        return; // Stay still during wind-up
    }

    // After wind-up, keep attack sprite visible during cooldown
    if (rank == ENEMY_ELITE && AIState == ENEMY_ATTACK && Anim.state != ATTACK)
        PlayAnimation(Anim, ATTACK, Anim.direction);

    Vector2 enemyCenter = GetCenter();
    Vector2 playerCenter = PlayerInstance.GetCenter();
    float dist = Vector2Distance(enemyCenter, playerCenter);
    float effectiveRange = GetEffectiveAttackRange();

    if (dist <= effectiveRange)
    {
        if (!PlayerWasInRange || AttackCooldownTimer <= 0)
        {
            if (rank == ENEMY_ELITE)
            {
                // Elite wind-up: stand still for 0.6s playing attack animation, then deal damage
                AttackWindUpTimer = 0.6f;
                PlayAnimation(Anim, ATTACK, Anim.direction);
                Anim.isAttacking = true;
            }
            else
            {
                PerformAttack();
            }
        }
        PlayerWasInRange = true;
    }
    else
    {
        PlayerWasInRange = false;
        // Sedikit buffer agar enemy tidak langsung keluar ATTACK state saat player mundur tipis
        float attackExitBuffer = 1.2f;
        if (dist > effectiveRange * attackExitBuffer)
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
    AudioManager::PlaySFX("attack");
    HitFlashTimer = 0.15f;
    HealthBarTimer = HealthBarDuration;
    // Boss immune knockback saat ability charge/slam
    if (!(rank == ENEMY_BOSS && (AIState == ENEMY_ABILITY1 || AIState == ENEMY_ABILITY2)))
        KnockbackVelocity = Vector2Scale(knockback, 5.0f);
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
        Color tint = WHITE;
        if (HitFlashTimer > 0)
            tint = RED;
        DrawAnimation(Anim, tint, Def->Scale);
    }

    // Ability danger zone
    if (AttackWindUpTimer > 0)
    {
        if (AIState == ENEMY_ABILITY1)
        {
            if (rank == ENEMY_ELITE)
            {
                Rectangle zone = GetAbilityZone();
                DrawRectangleRec(zone, ColorAlpha(RED, 0.55f));
            }
            else if (rank == ENEMY_BOSS)
            {
                Vector2 ctr = GetCenter();
                DrawCircleV(ctr, 190.0f, ColorAlpha(ORANGE, 0.35f));
                DrawCircleLinesV(ctr, 190.0f, ColorAlpha(RED, 0.7f));
            }
        }
        else if (AIState == ENEMY_ABILITY2 && rank == ENEMY_BOSS)
        {
            // Visual charge direction
            Vector2 ctr = GetCenter();
            Vector2 d = {0, 0};
            if (Anim.direction == LEFT)  d.x = -1;
            if (Anim.direction == RIGHT) d.x = 1;
            if (Anim.direction == UP)    d.y = -1;
            if (Anim.direction == DOWN)  d.y = 1;
            Vector2 end = Vector2Add(ctr, Vector2Scale(d, 200.0f));
            DrawLineEx(ctr, end, 6.0f, ColorAlpha(YELLOW, 0.5f));
            DrawCircleV(end, 8.0f, ColorAlpha(RED, 0.7f));
        }
    }

    // Health bar tampil saat agresif atau setelah kena damage (boss pake bar sendiri)
    if (rank != ENEMY_BOSS && (HealthBarTimer > 0 || AIState == ENEMY_CHASE || AIState == ENEMY_ATTACK))
    {
        int hpBarX = 4, hpBarY = 38, hpBarW = 24, hpBarH = 4;
        DrawRectangle((int)Position.x + hpBarX, (int)Position.y + hpBarY, hpBarW, hpBarH, BLACK);
        DrawRectangle((int)Position.x + hpBarX, (int)Position.y + hpBarY, (int)(hpBarW * (Health / MaxHealth)), hpBarH, RED);
    }

    if (isDebugMode)
    {
        Vector2 enemyCenter = GetCenter();
        DrawCircleLinesV(enemyCenter, DetectionRange, Fade(GRAY, 0.6f));
        DrawCircleLinesV(enemyCenter, GetEffectiveAttackRange(), GREEN);
        DrawCircleLinesV(enemyCenter, Def->stats.attackRange, Fade(RED, 0.3f));
        DrawRectangleLinesEx(GetHitbox(), 1.0f, VIOLET);
        DrawRectangleLinesEx(GetHurtbox(), 1.0f, YELLOW);

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
 * @brief Hapus semua enemy aktif dari entity manager.
 */

void ClearEnemies()
{
    // Boss music di-handle otomatis oleh UpdateBossMusic() tiap frame
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
        return &it->second;

    // Coba tanpa suffix _boss, _elite (e.g. "wolf_boss" → "wolf")
    for (const auto &suffix : {"_boss", "_elite"})
    {
        if (lowerName.size() > strlen(suffix) &&
            lowerName.substr(lowerName.size() - strlen(suffix)) == suffix)
        {
            std::string base = lowerName.substr(0, lowerName.size() - strlen(suffix));
            auto it2 = loadedAnimationSets.find(base);
            if (it2 != loadedAnimationSets.end())
                return &it2->second;
        }
    }

    it = loadedAnimationSets.find("slime");
    if (it != loadedAnimationSets.end())
        return &it->second;
    return nullptr;
}

/**
 * @brief Push enemy keluar dari collision dinding setelah spawn.
 * Coba offset bertahap dalam pola cross sampai dapat posisi aman.
 */
static void PushOutOfWalls(Enemy *enemy)
{
    if (!enemy)
        return;
    Vector2 pos = enemy->Position;
    if (IsPositionSafe(pos, enemy->HitboxWidth, enemy->HitboxHeight, enemy->HitboxOffsetX, enemy->HitboxOffsetY))
        return;

    float offsets[] = {4, 8, 12, 16, 20, 24, 28, 32, 40, 48};
    for (float o : offsets)
    {
        Vector2 tries[] = {
            {pos.x + o, pos.y},
            {pos.x - o, pos.y},
            {pos.x, pos.y + o},
            {pos.x, pos.y - o},
            {pos.x + o, pos.y + o},
            {pos.x - o, pos.y - o},
            {pos.x + o, pos.y - o},
            {pos.x - o, pos.y + o},
        };
        for (Vector2 t : tries)
        {
            if (IsPositionSafe(t, enemy->HitboxWidth, enemy->HitboxHeight, enemy->HitboxOffsetX, enemy->HitboxOffsetY))
            {
                enemy->Position = t;
                enemy->Anim.position = t;
                enemy->SpawnPoint = {t.x + enemy->HitboxWidth / 2.0f + enemy->HitboxOffsetX,
                                     t.y + enemy->HitboxHeight / 2.0f + enemy->HitboxOffsetY};
                return;
            }
        }
    }
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

    uint64_t dSeed = g_SeedManager.IsRunActive()
        ? (uint64_t)g_SeedManager.GetSeed(g_SeedManager.GetCurrentStage()) : 0;

    for (int i = 0; i < count; i++)
    {
        std::string picked = pool[pickDist(rng)];
        const EnemyDefinition &def = enemyData.Get(picked);

        Vector2 spawnPos = {center.x + offsetDist(rng), center.y + offsetDist(rng)};

        Enemy *enemy = new Enemy();
        enemy->Init(spawnPos, picked.c_str(), obj->id, def);
        enemy->SetUUID(GenerateDeterministicUUID(dSeed, obj->id, picked, i));
        PushOutOfWalls(enemy);
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

    uint64_t dSeed = g_SeedManager.IsRunActive()
        ? (uint64_t)g_SeedManager.GetSeed(g_SeedManager.GetCurrentStage()) : 0;

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
        enemy->SetUUID(GenerateDeterministicUUID(dSeed, obj->id, enemyName, i));
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

    uint64_t dSeed = g_SeedManager.IsRunActive()
        ? (uint64_t)g_SeedManager.GetSeed(g_SeedManager.GetCurrentStage()) : 0;

    Enemy *enemy = new Enemy();
    enemy->Init(spawnPos, picked.c_str(), obj->id, def);
    enemy->SetUUID(GenerateDeterministicUUID(dSeed, obj->id, picked, 0));
    PushOutOfWalls(enemy);
    enemy->SetReturnFlowField(&spawnFlowFields[obj->id].field);
    Entities::AddDynamic(enemy);
}

/**
 * @brief Spawn 2-3 enemy tutorial di posisi object pinpoint.
 * @param obj Object spawn tutorial dari Tiled
 * @note Fungsi terpisah dari SpawnAtPoint agar tidak mengubah
 *       logika spawn existing. Hanya dipakai di map tutorial.
 */
void SpawnTutorialEnemy(const MapObject *obj)
{
    if (!obj)
        return;

    auto pool = GetNamesByRank(ENEMY_NORMAL);
    if (pool.empty())
        return;

    std::mt19937 rng(obj->id);
    std::uniform_int_distribution<int> pickDist(0, (int)pool.size() - 1);
    std::uniform_int_distribution<int> countDist(SPAWN_PINPOINT_TUTORIAL_MIN, SPAWN_PINPOINT_TUTORIAL_MAX);
    std::uniform_real_distribution<float> offsetDist(-SEPARATION_RADIUS, SEPARATION_RADIUS);

    int count = countDist(rng);

    Vector2 center = {obj->bounds.x + obj->bounds.width / 2.0f,
                      obj->bounds.y + obj->bounds.height / 2.0f};
    if (spawnFlowFields.find(obj->id) == spawnFlowFields.end())
        BuildSpawnFlowFields(center, obj->id, tilesonMap->width, tilesonMap->height);

    uint64_t dSeed = g_SeedManager.IsRunActive()
        ? (uint64_t)g_SeedManager.GetSeed(g_SeedManager.GetCurrentStage()) : 0;

    for (int i = 0; i < count; i++)
    {
        std::string picked = pool[pickDist(rng)];
        const EnemyDefinition &def = enemyData.Get(picked);

        Vector2 spawnPos = {center.x + offsetDist(rng), center.y + offsetDist(rng)};

        Enemy *enemy = new Enemy();
        enemy->Init(spawnPos, picked.c_str(), obj->id, def);
        enemy->SetUUID(GenerateDeterministicUUID(dSeed, obj->id, picked, i));
        PushOutOfWalls(enemy);
        enemy->SetReturnFlowField(&spawnFlowFields[obj->id].field);
        Entities::AddDynamic(enemy);
    }
}

/**
 * @brief Spawn semua enemy dari object spawn di map aktif.
 * @note Semua spawn point selalu di-spawn. Kematian per-instance enemy
 *       ditangani oleh ApplyPostSpawn/ApplyCheckpointData.
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
        // Per-enemy UUID tracking (not per-spawn-point), so all spawn points always spawn
        std::mt19937 rng;
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
        else if (obj->name == ENEMY_SPAWN_TUTORIAL_PIN_OBJECT_NAME)
        {
            SpawnTutorialEnemy(obj);
        }
    }
}
