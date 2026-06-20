/**
 * @file propsbehavior.cpp
 * @brief Implementasi Props & Trap Behavior System
 *
 * File ini berisi implementasi untuk semua interactable props dan trap di dungeon:
 * - ChestManager: spawn, interaksi, dan loot chest
 * - SpikeManager: spawn, update timer aktif/nonaktif, damage player & enemy
 * - BombManager: spawn, explode, chain reaction, damage player & enemy
 * - CrateManager: spawn, destroy, dan loot crate
 * - SignManager: spawn dan interaksi sign yang bisa dibaca player
 *
 * Semua manager di-spawn via SpawnObject() yang dipanggil saat map di-load.
 */

#include "propsbehavior.h"
#include "../../include/systems/audioManager.h"
#include "item.h"
#include "enemy.h"
#include "enemy_ai.h"
#include "entities.h"
#include "core/utils.h"
#include "game_debug.h"
#include "animation.h"
#include "combatTurn.h"
#include <sstream>

/*==============================================================================
 * Utility Functions
 *==============================================================================*/

/**
 * @brief Snap posisi ke grid tile terdekat
 * @param rawPos Posisi mentah dari Tiled
 * @return Posisi yang sudah di-snap ke kelipatan FRAME_SIZE
 */
Vector2 SnapToTileGrid(Vector2 rawPos)
{
    return {
        std::floor(rawPos.x / FRAME_SIZE) * FRAME_SIZE,
        std::floor(rawPos.y / FRAME_SIZE) * FRAME_SIZE};
}

/**
 * @brief Cek apakah titik hit berada dalam bounds object (dengan toleransi)
 * @param hitPos Posisi hit
 * @param bounds Bounding box object
 * @param threshold Toleransi expand ke semua sisi
 * @param outCenter Output posisi center bounds (opsional)
 * @return true jika hitPos masuk dalam expanded bounds
 */
bool IsHitInBounds(Vector2 hitPos, Rectangle bounds, float threshold, Vector2 *outCenter = nullptr)
{
    Rectangle expanded = {
        bounds.x - threshold,
        bounds.y - threshold,
        bounds.width + threshold * 2,
        bounds.height + threshold * 2};

    if (outCenter)
    {
        outCenter->x = bounds.x + bounds.width / 2;
        outCenter->y = bounds.y + bounds.height / 2;
    }

    return CheckCollisionPointRec(hitPos, expanded);
}

/**
 * @brief Hitung jarak dari titik hit ke center bounds
 * @param hitPos Posisi hit
 * @param bounds Bounding box object
 * @return Jarak ke center
 */
float DistToCenter(Vector2 hitPos, Rectangle bounds)
{
    Vector2 center = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    return Vector2Distance(hitPos, center);
}

/**
 * @brief Spawn semua object dari Tiled ke manager masing-masing
 *
 * Dipanggil sekali saat map selesai di-load. Mengambil object
 * berdasarkan type dari object layer dan mendistribusikannya
 * ke ChestManager, SpikeManager, BombManager, dan CrateManager.
 */
void SpawnObject()
{
    auto chestObjs = TiledHelper::GetObjectsByType(CHEST_TYPE_OBJECT_NAME);
    chestManager.SpawnChests(chestObjs);

    auto spikeObjs = TiledHelper::GetObjectsByType(SPIKE_TYPE_OBJECT_NAME);
    spikeManager.SpawnSpikes(spikeObjs);

    auto bombObjs = TiledHelper::GetObjectsByType(BOMB_TYPE_OBJECT_NAME);
    bombManager.SpawnBombs(bombObjs);

    auto crateObjs = TiledHelper::GetObjectsByType(CRATE_TYPE_OBJECT_NAME);
    crateManager.SpawnCrates(crateObjs);

    // Spawn barriers — gabungin biasa + boss
    auto barrierObjs = TiledHelper::GetObjectsByType(BARRIER_TYPE_OBJECT_NAME);
    auto bossBarrierObjs = TiledHelper::GetObjectsByType(BARRIER_BOSS_TYPE_OBJECT_NAME);
    barrierObjs.insert(barrierObjs.end(), bossBarrierObjs.begin(), bossBarrierObjs.end());
    barrierManager.SpawnBarriers(barrierObjs);

    auto signObjs = TiledHelper::GetObjectsByType(SIGN_TYPE_OBJECT_NAME);
    signManager.SpawnSigns(signObjs);
}

static bool g_PendingObstacleRebuild = false;

/**
 * @brief Trigger hit attack player ke semua props yang bisa bereaksi terhadap serangan.
 * @param attackHitbox Hitbox serangan player
 * @param playerBounds Bounding box player untuk efek props tertentu
 * @param player Pointer ke player
 */
void HitPropsByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player)
{
    g_PendingObstacleRebuild = false;

    // Build solid obstacles untuk LOS check (static walls + dynamic minus crate/bomb)
    std::vector<Rectangle> solidObstacles = gCollisionCache.rects;
    for (const auto &obs : DynamicObstacles)
    {
        if (!crateManager.IsCratePos(obs) && !bombManager.IsBombPos(obs))
            solidObstacles.push_back(obs);
    }

    bombManager.HitByAttack(attackHitbox, playerBounds, player, solidObstacles);

    crateManager.HitByAttack(attackHitbox, playerBounds, solidObstacles);

    if (g_PendingObstacleRebuild)
    {
        RebuildObstacleCache();
        g_PendingObstacleRebuild = false;
    }
}

/*==============================================================================
 * Helper: Spawn loot aman di sekitar object
 *==============================================================================*/

/**
 * @brief Spawn item di posisi random aman di sekitar pusat object
 *
 * Cari posisi dengan IsPositionSafe (hitbox 20×20 = ukuran item), retry
 * beberapa kali. Fallback ke pusat object jika semua gagal.
 * Final safety net tetap ada di ItemDataManager::CreateItem.
 *
 * @param center Posisi pusat object (chest/crate)
 * @param itemCount Jumlah item yang di-spawn
 * @param rng Random generator
 * @param spread Radius pencarian posisi dari center
 * @param category Kategori item yang di-spawn
 */
static void SpawnLootSafe(Vector2 center, int itemCount, std::mt19937 &rng,
                          float spread, ItemCategory category, Vector2 hitboxSize)
{
    float halfW = hitboxSize.x / 2.0f;
    float halfH = hitboxSize.y / 2.0f;
    int maxRetry = 15;

    for (int i = 0; i < itemCount; i++)
    {
        Vector2 spawnPos = center;
        for (int retry = 0; retry < maxRetry; retry++)
        {
            Vector2 candidate = {
                center.x + (float)GetRandomValue(-(int)spread, (int)spread),
                center.y + (float)GetRandomValue(-(int)spread, (int)spread)};
            Vector2 topLeft = {candidate.x - halfW, candidate.y - halfH};
            if (IsPositionSafe(topLeft, hitboxSize.x, hitboxSize.y, 0, 0))
            {
                spawnPos = candidate;
                break;
            }
        }
        itemData.SpawnItemAtLocation(spawnPos, &rng, category);
    }
}

/*==============================================================================
 * ChestManager Implementation
 *==============================================================================*/

ChestManager chestManager;

/**
 * @brief Spawn semua chest dari object layer Tiled
 *
 * Snap posisi ke tile grid, set state awal Closed.
 *
 * @param chestObjects Daftar pointer MapObject bertipe chest
 */
void ChestManager::SpawnChests(const std::vector<MapObject *> &chestObjects)
{
    chests.clear();
    for (auto *obj : chestObjects)
    {
        Vector2 snapped = SnapToTileGrid({obj->bounds.x, obj->bounds.y});

        TileObject c;
        c.name = obj->name;
        c.bounds = obj->bounds;
        c.position = snapped;

        if (consumedPositions.count(EncodePos(snapped)))
            c.state = ObjectState::Open; // udah pernah dibuka
        else
            c.state = ObjectState::Closed;

        chests.push_back(c);
        DynamicObstacles.push_back(c.bounds);
    }
}

/**
 * @brief Cari chest terdekat dari titik hit
 *
 * Menggunakan expanded bounds agar titik di tepi tetap terdeteksi.
 *
 * @param hitPos Posisi hit dari player
 * @param threshold Toleransi jarak ke tepi bounds
 * @return Pointer ke chest terdekat, nullptr jika tidak ada
 */
TileObject *ChestManager::FindChest(Vector2 hitPos, float threshold)
{
    TileObject *closest = nullptr;
    float minDist = threshold;

    for (auto &chest : chests)
    {
        if (IsHitInBounds(hitPos, chest.bounds, threshold))
        {
            float dist = DistToCenter(hitPos, chest.bounds);
            if (dist < minDist)
            {
                minDist = dist;
                closest = &chest;
            }
        }
    }
    return closest; // nullptr kalau gak ada dalam threshold
}

/**
 * @brief Trigger interaksi player dengan chest
 *
 * Cari chest di sekitar hitPos, buka jika masih Closed, lalu trigger loot.
 *
 * @param hitPos Posisi interaksi player
 */
void ChestManager::Interact(Vector2 hitPos)
{
    TileObject *chest = FindChest(hitPos);
    if (!chest || chest->state == ObjectState::Open)
        return;
    chest->state = ObjectState::Open;
    AudioManager::PlaySFX("chest");
    consumedPositions.insert(EncodePos(chest->position));
    TriggerLoot(*chest);
}

/**
 * @brief Spawn loot item secara random di sekitar chest yang dibuka
 *
 * Jumlah item 1-3, posisi di-offset random di sekitar chest.
 * Rarity system belum diimplementasi.
 *
 * @param chest TileObject chest yang baru dibuka
 */
void ChestManager::TriggerLoot(TileObject &chest)
{
    TraceLog(LOG_INFO, "Chest opened at (%.1f, %.1f)", chest.position.x, chest.position.y);

    int jumlahLoot = GetRandomValue(1, 3);
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));

    Vector2 itemSize = itemDefs.GetMaxHitboxForCategory(ITEM_ANY);
    SpawnLootSafe(chest.position, jumlahLoot, rng, chestSpread, ITEM_ANY, itemSize);
}

/**
 * @brief Render semua chest ke layar
 *
 * State Closed = BROWN, Open = WHITE. Placeholder, belum pakai sprite.
 */
int ChestManager::Render(Rectangle viewRect)
{
    int rendered = 0;
    for (auto &c : chests)
    {
        if (!CheckCollisionRecs(c.bounds, viewRect))
            continue;
        rendered++;

        Display display;
        display.position = c.position;

        if (c.state == ObjectState::Closed)
            DrawFrame("chestClosed", display);
        else
            DrawFrame("chestOpen", display);
    }
    return rendered;
}

/**
 * @brief Bersihkan semua data chest
 */
void ChestManager::Clear()
{
    chests.clear();
}

void ChestManager::ResetConsumed()
{
    consumedPositions.clear();
}

/*==============================================================================
 * SpikeManager Implementation
 *==============================================================================*/

SpikeManager spikeManager;

/**
 * @brief Generate seed dari nama object untuk randomisasi timer
 *
 * Spike dengan nama sama akan punya seed yang sama,
 * dikombinasikan dengan global time seed agar tetap bervariasi tiap run.
 *
 * @param name Nama object spike dari Tiled
 * @return Seed unsigned int hasil hash nama
 */

unsigned int SpikeManager::SeedFromName(const std::string &name)
{
    unsigned int seed = 0;
    for (char c : name)
        seed = seed * 31 + static_cast<unsigned int>(c);
    return seed;
}

/**
 * @brief Spawn semua spike dari object layer Tiled
 *
 * Tiap spike dapat durasi aktif/nonaktif yang di-randomisasi
 * menggunakan kombinasi global time seed dan name seed.
 * State awal: Inactive.
 *
 * @param spikeObjects Daftar pointer MapObject bertipe spike
 */
void SpikeManager::SpawnSpikes(const std::vector<MapObject *> &spikeObjects)
{
    spikes.clear();
    for (auto *obj : spikeObjects)
    {
        SpikeData data;
        data.tile.name = obj->name;
        data.tile.bounds = obj->bounds;
        data.tile.state = ObjectState::Inactive;
        data.tile.position = SnapToTileGrid({obj->bounds.x, obj->bounds.y});

        unsigned int globalSeed = static_cast<unsigned int>(time(nullptr));
        unsigned int nameSeed = SeedFromName(obj->name);

        std::mt19937 rng(globalSeed ^ nameSeed);
        std::uniform_real_distribution<float> activeDist(SPIKE_ACTIVE_MIN, SPIKE_ACTIVE_MAX);
        std::uniform_real_distribution<float> inactiveDist(SPIKE_INACTIVE_MIN, SPIKE_INACTIVE_MAX);

        data.activeDuration = activeDist(rng);
        data.inactiveDuration = inactiveDist(rng);
        data.activeTimer = 0.0f;
        data.inactiveTimer = data.inactiveDuration;
        data.damageCooldown = 0.0f;

        SetupCallbacks(data);
        spikes.push_back(data);
    }
}

/**
 * @brief Setup callback untuk event spike
 *
 * - onActivate: set state ke Active
 * - onDeactivate: set state ke Inactive
 * - onDamagePlayer: placeholder log damage
 *
 * @param spike SpikeData yang akan di-setup callbacknya
 */
void SpikeManager::SetupCallbacks(SpikeData &spike)
{
    spike.onActivate = [](TileObject &tile)
    {
        tile.state = ObjectState::Active;
        // TraceLog(LOG_INFO, "Spike '%s' activated", tile.name.c_str());
    };

    spike.onDeactivate = [](TileObject &tile)
    {
        tile.state = ObjectState::Inactive;
        // TraceLog(LOG_INFO, "Spike '%s' deactivated", tile.name.c_str());
    };

    spike.onDamagePlayer = [](TileObject &tile)
    {
        // placeholder, sambungin ke player health system nanti
        TraceLog(LOG_INFO, "Spike '%s' damaged player", tile.name.c_str());
    };
}

/**
 * @brief Update timer dan damage spike tiap frame
 *
 * Alur per spike:
 * - Inactive: countdown inactiveTimer, switch ke Active jika habis
 * - Active: countdown activeTimer, switch ke Inactive jika habis
 * - Active: cek collision dengan player dan enemy, apply damage dengan cooldown global
 *
 * @param deltaTime Waktu antar frame
 * @param playerBounds Bounding box player untuk cek collision
 * @param player Pointer ke player untuk apply damage
 */
void SpikeManager::Update(float deltaTime, Rectangle playerBounds, Player *player)
{
    globalPlayerDamageCooldown -= deltaTime;
    globalEnemyDamageCooldown -= deltaTime;

    for (auto &spike : spikes)
    {
        if (spike.tile.state == ObjectState::Inactive)
        {
            spike.inactiveTimer -= deltaTime;
            if (spike.inactiveTimer <= 0.0f)
            {
                spike.activeTimer = spike.activeDuration;
                spike.inactiveTimer = 0.0f;
                spike.onActivate(spike.tile);
            }
            continue; // skip sisanya
        }

        // dari sini udah pasti Active
        spike.activeTimer -= deltaTime;
        if (spike.activeTimer <= 0.0f)
        {
            spike.inactiveTimer = spike.inactiveDuration;
            spike.activeTimer = 0.0f;
            spike.onDeactivate(spike.tile);
            continue;
        }
        // damage player (skip during turn-based combat)
        if (CheckCollisionRecs(playerBounds, spike.tile.bounds) && globalPlayerDamageCooldown <= 0.0f && player)
        {
            globalPlayerDamageCooldown = SPIKE_DAMAGE_COOLDOWN;
            if (!TurnCombat::IsActive())
                player->TakeDamage(SPIKE_DAMAGE);
        }

        // damage enemy (skip during turn-based combat)
        if (globalEnemyDamageCooldown <= 0.0f && !TurnCombat::IsActive())
        {
            for (auto entity : Entities::GetRegistry())
            {
                Enemy *enemy = dynamic_cast<Enemy *>(entity);
                if (!enemy || !enemy->IsActive || enemy->Health <= 0)
                    continue;
                if (CheckCollisionRecs(spike.tile.bounds, enemy->GetHitbox()))
                {
                    globalEnemyDamageCooldown = SPIKE_DAMAGE_COOLDOWN;
                    enemy->TakeDamage(SPIKE_DAMAGE, {0, 0});
                }
            }
        }
    }
}

/**
 * @brief Render semua spike ke layar
 *
 * Active = RED, Inactive = GRAY. Placeholder, belum pakai sprite.
 */
int SpikeManager::Render(Rectangle viewRect)
{
    int rendered = 0;
    for (auto &spike : spikes)
    {
        if (!CheckCollisionRecs(spike.tile.bounds, viewRect))
            continue;
        rendered++;

        Display display;
        display.position = spike.tile.position;

        if (spike.tile.state == ObjectState::Active)
            DrawFrame("spikeActive", display);
        else
            DrawFrame("spikeInactive", display);
    }
    return rendered;
}

/**
 * @brief Bersihkan semua data spike
 */
void SpikeManager::Clear()
{
    spikes.clear();
}

/*==============================================================================
 * ExplosionUtils Implementation
 *==============================================================================*/

/**
 * @brief Cluster bomb positions berdasarkan jarak menggunakan BFS pada graph bomb
 */
std::vector<std::vector<Vector2>> ExplosionUtils::ClusterByDistance(
    const std::vector<Vector2> &bombPositions,
    float clusterRadius)
{
    std::vector<bool> visited(bombPositions.size(), false);
    std::vector<std::vector<Vector2>> clusters;

    for (size_t i = 0; i < bombPositions.size(); i++)
    {
        if (visited[i])
            continue;

        std::vector<Vector2> cluster;
        std::queue<size_t> q;
        q.push(i);
        visited[i] = true;

        while (!q.empty())
        {
            size_t idx = q.front();
            q.pop();
            cluster.push_back(bombPositions[idx]);

            for (size_t j = 0; j < bombPositions.size(); j++)
            {
                if (visited[j])
                    continue;
                if (Vector2Distance(bombPositions[idx], bombPositions[j]) <= clusterRadius)
                {
                    visited[j] = true;
                    q.push(j);
                }
            }
        }

        clusters.push_back(cluster);
    }

    return clusters;
}

/**
 * @brief Multi-source BFS flood fill dari seed tiles
 *
 * Standard 4 arah (atas, kanan, bawah, kiri). Tile yang overlap dengan obstacle
 * tidak di-expand. Boundary tile dicek dengan CheckCollisionCircleRec.
 */
// Helper: cek apakah satu tile overlap dengan obstacle
bool IsTileBlocked(int tileX, int tileY, const std::vector<Rectangle> &obstacles)
{
    Rectangle r = {(float)(tileX * FRAME_SIZE), (float)(tileY * FRAME_SIZE),
                   (float)FRAME_SIZE, (float)FRAME_SIZE};
    for (const auto &obs : obstacles)
        if (CheckCollisionRecs(r, obs))
            return true;
    return false;
}

bool IsLineBlockedByObstacles(
    const Vector2 &startTile,
    const Vector2 &endTile,
    const std::vector<Rectangle> &obstacles)
{
    if (startTile.x == endTile.x && startTile.y == endTile.y)
        return false;

    int x0 = (int)(startTile.x / FRAME_SIZE);
    int y0 = (int)(startTile.y / FRAME_SIZE);
    int x1 = (int)(endTile.x / FRAME_SIZE);
    int y1 = (int)(endTile.y / FRAME_SIZE);

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    bool isStart = true;

    while (x != x1 || y != y1)
    {
        if (!isStart)
        {
            if (IsTileBlocked(x, y, obstacles))
                return true;
        }

        int e2 = 2 * err;
        bool stepX = (e2 > -dy);
        bool stepY = (e2 < dx);

        // Diagonal step: cek cross tiles biar gak nyusup lewat celah diagonal
        if (stepX && stepY)
        {
            // Cross tiles = (x+sx, y) dan (x, y+sy)
            // Kalau KEDUANYA blocked → diagonal gak bisa lewat
            bool hBlocked = IsTileBlocked(x + sx, y, obstacles);
            bool vBlocked = IsTileBlocked(x, y + sy, obstacles);

            // Kalau SALAH SATU cross tile blocked → diagonal gak tembus
            // (nanganin stair pattern: tiap langkah diagonal, salah satu cross tile pasti obstacle)
            if (hBlocked || vBlocked)
                return true;
        }

        if (stepX) { err -= dy; x += sx; }
        if (stepY) { err += dx; y += sy; }
        isStart = false;
    }

    return false;
}

/**
 * @brief Unified radius + tile DDA line-of-sight check
 *
 * Step 1: nearest-point dalam radius
 * Step 2: tile DDA (Bresenham) — cek apakah ada obstacle di tile intermediate
 */
bool ExplosionUtils::CheckExplosionCircle(
    const Vector2 &bombCenter,
    float radius,
    const Rectangle &targetHitbox,
    const std::vector<Rectangle> &obstacles)
{
    // Step 1: nearest point di hitbox target
    float nearestX = Clamp(bombCenter.x, targetHitbox.x, targetHitbox.x + targetHitbox.width);
    float nearestY = Clamp(bombCenter.y, targetHitbox.y, targetHitbox.y + targetHitbox.height);
    Vector2 nearestPoint = {nearestX, nearestY};
    if (Vector2Distance(bombCenter, nearestPoint) > radius)
        return false;

    // Step 2: tile DDA line-of-sight
    Vector2 bombTile = SnapToTileGrid(bombCenter);
    Vector2 targetTile = SnapToTileGrid(nearestPoint);
    return !IsLineBlockedByObstacles(bombTile, targetTile, obstacles);
}

/*==============================================================================
 * BombManager Implementation
 *==============================================================================*/

BombManager bombManager;

/**
 * @brief Spawn semua bomb dari object layer Tiled
 *
 * Snap posisi ke tile grid, set state awal Active,
 * dan daftarkan ke DynamicObstacles untuk pathfinding enemy.
 *
 * @param bombObjects Daftar pointer MapObject bertipe bomb
 */
void BombManager::SpawnBombs(const std::vector<MapObject *> &bombObjects)
{
    bombs.clear();
    for (auto *obj : bombObjects)
    {
        Vector2 snapped = SnapToTileGrid({obj->bounds.x, obj->bounds.y});
        if (consumedPositions.count(EncodePos(snapped)))
            continue; // skip yang udah meledak

        BombData data;
        data.tile.name = obj->name;
        data.tile.bounds = obj->bounds;
        data.tile.state = ObjectState::Active;
        data.tile.position = SnapToTileGrid({obj->bounds.x, obj->bounds.y});
        data.tile.uuid = GenerateUUID();

        data.isAlive = true;
        data.isExploding = false;
        data.explosionTimer = 0.0f;

        bombs.push_back(data);
        DynamicObstacles.push_back(data.tile.bounds);
    }
}

/**
 * @brief Cari bomb terdekat dari titik hit
 *
 * @param hitPos Posisi hit
 * @param threshold Toleransi jarak ke tepi bounds
 * @return Pointer ke TileObject bomb terdekat, nullptr jika tidak ada
 */
TileObject *BombManager::FindBomb(Vector2 hitPos, float threshold)
{
    TileObject *closest = nullptr;
    float minDist = threshold;

    for (auto &bomb : bombs)
    {
        if (!bomb.isAlive)
            continue;
        if (IsHitInBounds(hitPos, bomb.tile.bounds, threshold))
        {
            float dist = DistToCenter(hitPos, bomb.tile.bounds);
            if (dist < minDist)
            {
                minDist = dist;
                closest = &bomb.tile;
            }
        }
    }
    return closest;
}

/**
 * @brief Trigger ledakan bomb yang terkena hitbox serangan player
 *
 * @param attackHitbox Hitbox serangan player
 * @param playerBounds Bounding box player
 * @param player Pointer ke player
 */
void BombManager::HitByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player,
                              const std::vector<Rectangle> &solidObstacles)
{
    Vector2 playerCenter = {playerBounds.x + playerBounds.width / 2.0f, playerBounds.y + playerBounds.height / 2.0f};

    for (auto &bomb : bombs)
    {
        if (!bomb.isAlive || bomb.isExploding)
            continue;
        if (!CheckCollisionAgainstRects(attackHitbox, {bomb.tile.bounds}))
            continue;

        // LOS check pake Bresenham tile DDA (handle stair pattern)
        if (!solidObstacles.empty())
        {
            Vector2 bombCenter = {
                bomb.tile.bounds.x + bomb.tile.bounds.width / 2.0f,
                bomb.tile.bounds.y + bomb.tile.bounds.height / 2.0f};
            if (IsLineBlockedByObstacles(playerCenter, bombCenter, solidObstacles))
                continue;
        }

        Explode(bomb, playerBounds, player);
    }
}

/**
 * @brief Trigger ledakan bomb
 *
 * Urutan proses:
 * 1. Kumpulin chain bomb (BFS pada graph bomb)
 * 2. Cluster chain bomb berdasarkan jarak
 * 3. Mark semua chain bomb + hapus dari DynamicObstacles
 * 4. Per cluster: multi-source BFS flood fill
 * 5. Per cluster: hancurkan crate yang overlap reachable tiles
 * 6. Per cluster: re-compute reachable tiles setelah crate hancur
 * 7. Per cluster: damage entity (player & enemy) via reachable + shadow
 *
 * @param bomb BombData yang akan diledakkan
 * @param playerBounds Bounding box player
 * @param player Pointer ke player untuk apply damage
 */
void BombManager::Explode(BombData &bomb, Rectangle playerBounds, Player *player)
{
    /*=== Fase 1: Kumpulin chain bomb ===*/
    std::vector<BombData *> chain;
    std::unordered_set<BombData *> visitedChain;
    std::queue<BombData *> qChain;
    qChain.push(&bomb);
    visitedChain.insert(&bomb);

    while (!qChain.empty())
    {
        BombData *current = qChain.front();
        qChain.pop();
        chain.push_back(current);

        Vector2 curCenter = {
            current->tile.position.x + FRAME_SIZE / 2.0f,
            current->tile.position.y + FRAME_SIZE / 2.0f};

        for (auto &other : bombs)
        {
            if (&other == current)
                continue;
            if (visitedChain.count(&other))
                continue;
            if (!other.isAlive || other.isExploding || other.isTriggered)
                continue;

            Vector2 otherCenter = {
                other.tile.position.x + FRAME_SIZE / 2.0f,
                other.tile.position.y + FRAME_SIZE / 2.0f};

            // Chain reaction: distance check (mirip IsInExplosionRadius)
            float nearestX = Clamp(curCenter.x, other.tile.bounds.x, other.tile.bounds.x + other.tile.bounds.width);
            float nearestY = Clamp(curCenter.y, other.tile.bounds.y, other.tile.bounds.y + other.tile.bounds.height);
            if (Vector2Distance(curCenter, {nearestX, nearestY}) <= BOMB_EXPLOSION_RADIUS)
            {
                visitedChain.insert(&other);
                qChain.push(&other);
            }
        }
    }

    /*=== Fase 2: Collect center positions & cluster ===*/
    std::vector<Vector2> chainCenters;
    chainCenters.reserve(chain.size());
    for (auto *cb : chain)
        chainCenters.push_back({
            cb->tile.position.x + FRAME_SIZE / 2.0f,
            cb->tile.position.y + FRAME_SIZE / 2.0f});

    auto clusters = ExplosionUtils::ClusterByDistance(chainCenters, BOMB_EXPLOSION_RADIUS * 2.0f);

    /*=== Fase 3: Mark exploding + hapus obstacles ===*/
    for (auto *cb : chain)
    {
        cb->tile.state = ObjectState::Inactive;
        cb->isExploding = true;
        cb->isTriggered = true;
        cb->explosionTimer = BOMB_EXPLOSION_DURATION;

        consumedPositions.insert(EncodePos(cb->tile.position));

        DynamicObstacles.erase(
            std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                           { return r.x == cb->tile.bounds.x && r.y == cb->tile.bounds.y; }),
            DynamicObstacles.end());
        MarkSpawnFlowFieldsDirty(cb->tile.position);
    }
    g_PendingObstacleRebuild = true;

    AudioManager::PlaySFX("explosion");

    /*=== Bangun daftar obstacle gabungan (dynamic + static collision) ===*/
    // Static collision dari Tiled object layer (COLLISION_LAYER_NAME)
    const auto &staticRects = gCollisionCache.rects;

    // Solid obstacles buat crate shadow check (dynamic minus crate + static)
    std::vector<Rectangle> solidObstacles;
    solidObstacles.reserve(DynamicObstacles.size() + staticRects.size());
    for (const auto &obs : DynamicObstacles)
    {
        if (crateManager.IsCratePos(obs))
            continue;
        solidObstacles.push_back(obs);
    }
    solidObstacles.insert(solidObstacles.end(), staticRects.begin(), staticRects.end());

    // Full obstacles buat entity damage check (dynamic + static)
    std::vector<Rectangle> allObstacles;
    allObstacles.reserve(DynamicObstacles.size() + staticRects.size());
    allObstacles.insert(allObstacles.end(), DynamicObstacles.begin(), DynamicObstacles.end());
    allObstacles.insert(allObstacles.end(), staticRects.begin(), staticRects.end());

    /*=== Fase 4: Crate destruction (global) — radius + shadow thd solidObstacles ===*/
    for (const auto &bombCenter : chainCenters)
        crateManager.HitByExplosion(bombCenter, BOMB_EXPLOSION_RADIUS, solidObstacles);

    /*=== Fase 5-6: Per cluster — entity damage accumulation ===*/
    for (auto &cluster : clusters)
    {
        if (cluster.empty())
            continue;

        // Akumulasi hit count per entity biar TakeDamage dipanggil sekali
        int playerHitCount = 0;
        std::unordered_map<Enemy *, int> enemyHitCounts;

        for (const auto &bombCenter : cluster)
        {
            if (player && ExplosionUtils::CheckExplosionCircle(
                              bombCenter, BOMB_EXPLOSION_RADIUS,
                              playerBounds, allObstacles))
                playerHitCount++;

            for (auto entity : Entities::GetRegistry())
            {
                Enemy *enemy = dynamic_cast<Enemy *>(entity);
                if (!enemy)
                    continue;
                if (!enemy->IsActive || enemy->Health <= 0)
                    continue;

                if (ExplosionUtils::CheckExplosionCircle(
                        bombCenter, BOMB_EXPLOSION_RADIUS,
                        enemy->GetHitbox(), allObstacles))
                    enemyHitCounts[enemy]++;
            }
        }

        // Apply damage sekali per entity — skip during turn-based combat
        if (!TurnCombat::IsActive())
        {
            if (playerHitCount > 0)
                player->TakeDamage(static_cast<float>(playerHitCount) * BOMB_DAMAGE);

            for (auto &[enemy, count] : enemyHitCounts)
                enemy->TakeDamage(static_cast<float>(count) * BOMB_DAMAGE, {0, 0});
        }
    }
}

/**
 * @brief Update state semua bomb tiap frame
 *
 * Countdown explosionTimer untuk bomb yang sedang meledak.
 * Bomb yang explosionTimer-nya habis di-set isAlive = false,
 * lalu dihapus dari vector di akhir update.
 *
 * @param deltaTime Waktu antar frame
 * @param playerBounds Bounding box player
 * @param player Pointer ke player
 */
void BombManager::Update(float deltaTime, Rectangle playerBounds, Player *player)
{
    for (auto &bomb : bombs)
    {
        if (!bomb.isAlive)
            continue;

        if (bomb.isExploding)
        {
            bomb.explosionTimer -= deltaTime;
            if (bomb.explosionTimer <= 0.0f)
                bomb.isAlive = false;
        }
    }

    // hapus bomb yang udah mati
    bombs.erase(
        std::remove_if(bombs.begin(), bombs.end(), [](const BombData &bomb)
                       { return !bomb.isAlive; }),
        bombs.end());

}

/**
 * @brief Render semua bomb ke layar
 *
 * Exploding = lingkaran orange transparan radius BOMB_EXPLOSION_RADIUS.
 * Idle = kotak RED. Placeholder, belum pakai sprite.
 */
int BombManager::Render(Rectangle viewRect)
{
    int rendered = 0;
    for (auto &bomb : bombs)
    {
        if (!bomb.isAlive)
            continue;
        if (!CheckCollisionRecs(bomb.tile.bounds, viewRect))
            continue;
        rendered++;

        if (bomb.isExploding)
        {
            Vector2 bombCenter = {
                bomb.tile.position.x + FRAME_SIZE / 2.0f,
                bomb.tile.position.y + FRAME_SIZE / 2.0f};
            float progress = (BOMB_EXPLOSION_DURATION - bomb.explosionTimer) / BOMB_EXPLOSION_DURATION;
            Explosion(bombCenter, BOMB_EXPLOSION_RADIUS, progress);
        }
        else
        {
            if (isDebugMode)
            {
                Vector2 bombCenter = {
                    bomb.tile.position.x + FRAME_SIZE / 2.0f,
                    bomb.tile.position.y + FRAME_SIZE / 2.0f};
                DrawCircleV(bombCenter, BOMB_EXPLOSION_RADIUS, Fade(RED, 0.15f));
                DrawCircleLinesV(bombCenter, BOMB_EXPLOSION_RADIUS, Fade(RED, 0.5f));
            }
            Display display;
            display.position = bomb.tile.position;
            DrawFrame("bomb", display);
        }
    }

    return rendered;
}

/**
 * @brief Bersihkan semua data bomb
 */
void BombManager::Clear()
{
    bombs.clear();
}

void BombManager::ResetConsumed()
{
    consumedPositions.clear();
}

bool BombManager::IsBombPos(const Rectangle &bounds) const
{
    for (const auto &b : bombs)
        if (b.isAlive && !b.isExploding && !b.isTriggered && b.tile.bounds.x == bounds.x && b.tile.bounds.y == bounds.y)
            return true;
    return false;
}

/*==============================================================================
 * CrateManager Implementation
 *==============================================================================*/

CrateManager crateManager;

/**
 * @brief Spawn semua crate dari object layer Tiled.
 * @param crateObjects Daftar pointer MapObject bertipe crate
 * @note Crate yang posisinya sudah tercatat hancur tidak akan di-spawn ulang.
 */
void CrateManager::SpawnCrates(const std::vector<MapObject *> &crateObjects)
{
    crates.clear();
    for (auto *obj : crateObjects)
    {
        Vector2 snapped = SnapToTileGrid({obj->bounds.x, obj->bounds.y});
        if (consumedPositions.count(EncodePos(snapped)))
            continue; // skip yang udah hancur

        CrateData data;
        data.tile.name = obj->name;
        data.tile.bounds = obj->bounds;
        data.tile.position = snapped;
        data.tile.state = ObjectState::Active;
        data.tile.uuid = GenerateUUID();
        data.isAlive = true;

        crates.push_back(data);
        DynamicObstacles.push_back(data.tile.bounds);
    }
}

/**
 * @brief Spawn semua crate dari object layer Tiled.
 * @param crateObjects Daftar pointer MapObject bertipe crate
 * @note Crate yang posisinya sudah tercatat hancur tidak akan di-spawn ulang.
 */
void CrateManager::HitByAttack(Rectangle attackHitbox, Rectangle playerBounds,
                               const std::vector<Rectangle> &solidObstacles)
{
    Vector2 playerCenter = {playerBounds.x + playerBounds.width / 2.0f, playerBounds.y + playerBounds.height / 2.0f};

    std::vector<Rectangle> toRemove;
    bool anyDestroyed = false;

    for (auto &crate : crates)
    {
        if (!crate.isAlive)
            continue;
        if (!CheckCollisionRecs(attackHitbox, crate.tile.bounds))
            continue;

        // LOS check pake Bresenham tile DDA (handle stair pattern)
        if (!solidObstacles.empty())
        {
            Vector2 crateCenter = {
                crate.tile.bounds.x + crate.tile.bounds.width / 2.0f,
                crate.tile.bounds.y + crate.tile.bounds.height / 2.0f};
            if (IsLineBlockedByObstacles(playerCenter, crateCenter, solidObstacles))
                continue;
        }

        // Hancurkan crate, tapi tunda DynamicObstacles erase biar batch
        crate.tile.state = ObjectState::Inactive;
        crate.isAlive = false;
        consumedPositions.insert(EncodePos(crate.tile.position));
        toRemove.push_back(crate.tile.bounds);
        MarkSpawnFlowFieldsDirty(crate.tile.position);
        TriggerLoot(crate.tile);
        anyDestroyed = true;
    }

    if (anyDestroyed)
    {
        AudioManager::PlaySFX("crate"); // cukup sekali

        // Batch: 1× DynamicObstacles erase untuk semua crate
        DynamicObstacles.erase(
            std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                           {
                for (const auto &rem : toRemove)
                    if (r.x == rem.x && r.y == rem.y) return true;
                return false; }),
            DynamicObstacles.end());
        g_PendingObstacleRebuild = true;
    }
}

/**
 * @brief Hancurkan crate yang kena ledakan bomb (radius + shadow terhadap solid obstacles)
 * @param bombCenter Posisi center bomb
 * @param radius Radius ledakan
 * @param solidObstacles Daftar obstacle solid (barrier, wall — tanpa crate/bomb)
 */
void CrateManager::HitByExplosion(const Vector2 &bombCenter, float radius, const std::vector<Rectangle> &solidObstacles)
{
    std::vector<Rectangle> toRemove;
    bool anyDestroyed = false;

    for (auto &crate : crates)
    {
        if (!crate.isAlive)
            continue;

        if (!ExplosionUtils::CheckExplosionCircle(bombCenter, radius, crate.tile.bounds, solidObstacles))
            continue;

        crate.tile.state = ObjectState::Inactive;
        crate.isAlive = false;
        consumedPositions.insert(EncodePos(crate.tile.position));
        toRemove.push_back(crate.tile.bounds);
        MarkSpawnFlowFieldsDirty(crate.tile.position);
        TriggerLoot(crate.tile);
        anyDestroyed = true;
    }

    if (anyDestroyed)
    {
        AudioManager::PlaySFX("crate");
        DynamicObstacles.erase(
            std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                           {
                for (const auto &rem : toRemove)
                    if (r.x == rem.x && r.y == rem.y) return true;
                return false; }),
            DynamicObstacles.end());
        g_PendingObstacleRebuild = true;
    }
}

/**
 * @brief Hapus crate yang sudah dihancurkan dari daftar runtime.
 */
void CrateManager::Update()
{
    crates.erase(
        std::remove_if(crates.begin(), crates.end(), [](const CrateData &crate)
                       { return !crate.isAlive; }),
        crates.end());
}

/**
 * @brief Hancurkan crate dan update obstacle/pathfinding terkait.
 * @param crate Data crate yang akan dihancurkan
 * @note Memanggil RebuildObstacleCache setelah crate dihapus dari DynamicObstacles.
 */
void CrateManager::Destroy(CrateData &crate)
{
    crate.tile.state = ObjectState::Inactive;
    crate.isAlive = false;
    consumedPositions.insert(EncodePos(crate.tile.position));

    AudioManager::PlaySFX("crate");

    DynamicObstacles.erase(
        std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                       { return r.x == crate.tile.bounds.x && r.y == crate.tile.bounds.y; }),
        DynamicObstacles.end());
    MarkSpawnFlowFieldsDirty(crate.tile.position);
    g_PendingObstacleRebuild = true;

    TriggerLoot(crate.tile);
}

/**
 * @brief Roll peluang drop loot dari crate yang dihancurkan.
 * @param crate TileObject crate yang dihancurkan
 */
void CrateManager::TriggerLoot(TileObject &crate)
{
    float roll = (float)GetRandomValue(0, 99) / 100.0f;
    if (roll >= CRATE_LOOT_CHANCE)
        return;

    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));

    Vector2 itemSize = itemDefs.GetMaxHitboxForCategory(ITEM_POTION);
    SpawnLootSafe(crate.position, 1, rng, crateSpread, ITEM_POTION, itemSize);
}

/**
 * @brief Render semua crate yang terlihat di viewport.
 * @param viewRect Area kamera/viewport aktif
 * @return Jumlah crate yang berhasil dirender
 */
int CrateManager::Render(Rectangle viewRect)
{
    int rendered = 0;
    for (auto &crate : crates)
    {
        if (!crate.isAlive)
            continue;
        if (!CheckCollisionRecs(crate.tile.bounds, viewRect))
            continue;
        Display display;
        display.position = crate.tile.position;
        DrawFrame("crate", display);
        rendered++;
    }
    return rendered;
}

/**
 * @brief Bersihkan semua data crate.
 */
void CrateManager::Clear()
{
    crates.clear();
}

bool CrateManager::IsCratePos(const Rectangle &bounds) const
{
    for (const auto &crate : crates)
        if (crate.isAlive && crate.tile.bounds.x == bounds.x && crate.tile.bounds.y == bounds.y)
            return true;
    return false;
}

void CrateManager::ResetConsumed()
{
    consumedPositions.clear();
}

/*==============================================================================
 * BarrierManager Implementation
 *==============================================================================*/

BarrierManager barrierManager;

/**
 * @brief Spawn semua barrier dari object layer Tiled
 *
 * 1. Cek apakah map ini punya boss spawn — untuk mode re-lock
 * 2. Cari object "boss" (pass) sebagai detektor area room boss
 * 3. Snap posisi ke tile grid, daftarkan ke DynamicObstacles
 *
 * @param barrierObjects Daftar pointer MapObject bertipe barrier
 */
void BarrierManager::SpawnBarriers(const std::vector<MapObject *> &barrierObjects)
{
    barriers.clear();
    isBossMap = false;
    bossStageBounds = {0};

    // Cek apakah ada object boss_stage — kalo ada, ini boss map
    isBossMap = false;
    bossStageBounds = {0};
    auto stageObjs = TiledHelper::GetObjectsByType(BOSS_STAGE_TYPE_OBJECT_NAME);
    if (!stageObjs.empty())
    {
        isBossMap = true;
        bossStageBounds = stageObjs[0]->bounds;
        TraceLog(LOG_INFO, "BarrierManager: boss_stage detected at (%.0f,%.0f w=%.0f h=%.0f)",
                 bossStageBounds.x, bossStageBounds.y, bossStageBounds.width, bossStageBounds.height);
    }

    // totalEnemyCount di-capture pas Update pertama, karena enemy belum di-spawn pas SpawnObject
    totalEnemyCount = 0;
    hasCapturedCount = false;

    // JANGAN reset cleared/hasReLocked — state ini bisa di-set oleh LoadRuntimeState sebelumnya
    // Kalo sudah cleared (dari save load), skip spawn barrier sama sekali
    if (cleared)
    {
        TraceLog(LOG_INFO, "BarrierManager: stage already cleared, skipping barrier spawn");
        return;
    }

    for (auto *obj : barrierObjects)
    {
        Vector2 snapped = SnapToTileGrid({obj->bounds.x, obj->bounds.y});

        BarrierData data;
        data.tile.name = obj->name;
        data.tile.bounds = obj->bounds;
        data.tile.position = snapped;
        data.tile.state = ObjectState::Active;
        data.isActive = true;
        data.isBoss = (obj->type == BARRIER_BOSS_TYPE_OBJECT_NAME);

        barriers.push_back(data);
        DynamicObstacles.push_back(data.tile.bounds);
    }
}

/**
 * @brief Update tiap frame — cek kill threshold & boss room state
 *
 * Non-boss map: barrier hilang permanen setelah 90% enemy mati.
 * Boss map:     barrier buka (90%) → re-lock pas player masuk → buka lagi setelah boss mati.
 */
void BarrierManager::Update()
{
    if (barriers.empty())
        return;

    // Delayed capture totalEnemyCount — enemy belum di-spawn pas SpawnObject
    if (!hasCapturedCount)
    {
        // Di boss map: exclude boss dari hitungan threshold
        totalEnemyCount = 0;
        for (auto *enemy : Entities::GetEnemyRegistry())
        {
            if (isBossMap && enemy->rank == ENEMY_BOSS)
                continue;
            totalEnemyCount++;
        }
        hasCapturedCount = true;
        TraceLog(LOG_INFO, "BarrierManager: total enemy count = %d (boss excluded: %s)",
                 totalEnemyCount, isBossMap ? "yes" : "no");
    }

    // Kalo gak ada enemy sama sekali, langsung clear
    if (totalEnemyCount == 0)
    {
        RemoveAllBarriers();
        return;
    }

    // Hitung jumlah enemy non-boss yang sudah mati (boss map: boss excluded)
    int deadCount = 0;
    int bossAlive = 0;
    for (auto *enemy : Entities::GetEnemyRegistry())
    {
        if (isBossMap && enemy->rank == ENEMY_BOSS)
        {
            if (enemy->IsActive)
                bossAlive++;
            continue;
        }
        if (!enemy->IsActive)
            deadCount++;
    }

    // Trace tiap kali ada enemy baru yang mati
    if (deadCount != prevDeadCount)
    {
        int alive = totalEnemyCount - deadCount;
        TraceLog(LOG_INFO, "BarrierManager: enemy killed — remaining %d / total %d", alive, totalEnemyCount);
        prevDeadCount = deadCount;
    }

    if (isBossMap)
    {
        /*-------- Boss map: 3-step flow --------*/

        Rectangle playerBounds = PlayerInstance.GetHitbox();

        if (!cleared && !hasReLocked && totalEnemyCount > 0 &&
            (float)deadCount / (float)totalEnemyCount >= KILL_THRESHOLD)
        {
            // Step 1 — threshold terpenuhi, barrier buka
            RemoveAllBarriers();
            TraceLog(LOG_INFO, "BarrierManager: boss barrier opened (threshold met)");
        }

        if (cleared && !hasReLocked && bossStageBounds.width > 0 && bossStageBounds.height > 0)
        {
            // Step 2 — player masuk boss room → re-lock
            if (CheckCollisionRecs(playerBounds, bossStageBounds))
            {
                ReLockBarriers();
                hasReLocked = true;
                TraceLog(LOG_INFO, "BarrierManager: boss barrier re-locked (player entered boss room)");
            }
        }

        if (hasReLocked && !cleared)
        {
            // Step 3 — cek apa boss udah mati
            if (bossAlive == 0)
            {
                RemoveAllBarriers();
                pendingReLock = false;
                TraceLog(LOG_INFO, "BarrierManager: boss barrier unlocked (boss defeated)");
            }
        }

        // Step 4 — retry barrier yang di-skip karena overlap player
        // All-or-nothing: kalo masih ada 1 aja yang overlap → jangan lock satupun
        if (hasReLocked && pendingReLock)
        {
            bool stillOverlap = false;
            for (auto &b : barriers)
            {
                if (b.isActive)
                    continue;
                if (CheckCollisionRecs(playerBounds, b.tile.bounds))
                {
                    stillOverlap = true;
                    break;
                }
            }

            if (!stillOverlap)
            {
                for (auto &b : barriers)
                {
                    if (b.isActive)
                        continue;
                    b.isActive = true;
                    DynamicObstacles.push_back(b.tile.bounds);
                }
                pendingReLock = false;
                RebuildObstacleCache();
                TraceLog(LOG_INFO, "BarrierManager: all pending barriers locked");
            }
        }

        // Step 5 — player keluar area boss room → unlock barrier
        // Guard: kalo player mati/respawn di luar, barrier gak bakal stuck lock
        if (hasReLocked && !cleared)
        {
            if (!CheckCollisionRecs(playerBounds, bossStageBounds))
            {
                for (auto &b : barriers)
                {
                    if (!b.isActive)
                        continue;
                    b.isActive = false;
                    DynamicObstacles.erase(
                        std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(),
                                       [&](const Rectangle &r)
                                       {
                                           return r.x == b.tile.bounds.x && r.y == b.tile.bounds.y;
                                       }),
                        DynamicObstacles.end());
                }
                hasReLocked = false;
                cleared = true;
                pendingReLock = false;
                RebuildObstacleCache();
                TraceLog(LOG_INFO, "BarrierManager: boss barrier unlocked (player left boss room)");
            }
        }
    }
    else
    {
        /*-------- Normal map: unlock once threshold met --------*/
        if (!cleared && totalEnemyCount > 0 &&
            (float)deadCount / (float)totalEnemyCount >= KILL_THRESHOLD)
        {
            RemoveAllBarriers();
            TraceLog(LOG_INFO, "BarrierManager: barrier opened (threshold met)");
        }
    }
}

/**
 * @brief Render barrier sebagai rectangle semi-transparan
 *
 * Sementara pake warna YELLONG selama belum ada texture pack.
 *
 * @param viewRect Area kamera/viewport aktif
 * @return Jumlah barrier yang berhasil dirender
 */
int BarrierManager::Render(Rectangle viewRect)
{
    int rendered = 0;

    // Pass 1: Render barrierDown and barrierUp1 (the base layer)
    for (auto &b : barriers)
    {
        if (!CheckCollisionRecs(b.tile.bounds, viewRect))
            continue;

        Display display;
        display.position = b.tile.position;

        if (b.isActive)
        {
            DrawFrame("barrierUp1", display);
        }
        else
        {
            DrawFrame("barrierDown", display);
        }
        rendered++;
    }

    // Pass 2: Render barrierUp2 (top layer) so it is not obstructed by adjacent barrierUp1s
    for (auto &b : barriers)
    {
        if (!b.isActive)
            continue;

        // We expand the viewRect slightly upwards to ensure barrierUp2 is drawn even if the base is slightly off-screen
        Rectangle expandedView = viewRect;
        expandedView.y -= FRAME_SIZE;
        expandedView.height += FRAME_SIZE;

        if (!CheckCollisionRecs(b.tile.bounds, expandedView))
            continue;

        Display display;
        display.position = b.tile.position;
        display.position.y -= FRAME_SIZE;
        DrawFrame("barrierUp2", display);
    }

    return rendered;
}

/**
 * @brief Hapus semua barrier dari DynamicObstacles
 */
void BarrierManager::RemoveAllBarriers()
{
    for (auto &b : barriers)
    {
        if (!b.isActive)
            continue;
        b.isActive = false;
        DynamicObstacles.erase(
            std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(),
                           [&](const Rectangle &r)
                           {
                               return r.x == b.tile.bounds.x && r.y == b.tile.bounds.y;
                           }),
            DynamicObstacles.end());
    }
    cleared = true;
    RebuildObstacleCache();
}

/**
 * @brief Pasang ulang barrier (re-lock) — khusus boss room
 *
 * Setelah player masuk room boss, barrier ditutup lagi
 * dan baru akan terbuka setelah boss mati.
 */
void BarrierManager::ReLockBarriers()
{
    Rectangle playerBounds = PlayerInstance.GetHitbox();

    // Cek apakah ADA barrier yang overlap dengan player
    for (auto &b : barriers)
    {
        if (b.isActive)
            continue;
        if (CheckCollisionRecs(playerBounds, b.tile.bounds))
        {
            // Ada overlap → jangan lock satupun — tunggu sampe player keluar
            pendingReLock = true;
            cleared = false;
            TraceLog(LOG_INFO, "BarrierManager: barriers deferred (player overlap barrier)");
            return;
        }
    }

    // Gak ada overlap sama sekali → lock semua barrier
    for (auto &b : barriers)
    {
        if (b.isActive)
            continue;
        b.isActive = true;
        DynamicObstacles.push_back(b.tile.bounds);
    }
    cleared = false;
    pendingReLock = false;
    RebuildObstacleCache();
    TraceLog(LOG_INFO, "BarrierManager: all barriers locked");
}

/**
 * @brief Bersihkan semua data barrier
 */
void BarrierManager::Clear()
{
    barriers.clear();
    cleared = false; // Default: barrier belum di-clear — di-set ulang sama LoadRuntimeState kalo ada save
    isBossMap = false;
    hasReLocked = false;
    pendingReLock = false;
    bossStageBounds = {0};
    totalEnemyCount = 0;
    prevDeadCount = 0;
    hasCapturedCount = false;
}

/*==============================================================================
 * SignManager Implementation
 *==============================================================================*/

SignManager signManager;

/**
 * @brief Split teks dialog jadi baris-baris
 *
 * Prioritas delimiter: | dulu. Kalo gak ada, fallback ke \n.
 * Tiap baris di-trim whitespace-nya.
 *
 * @param text Teks mentah dari custom property Tiled
 * @return Vektor baris dialog
 */
std::vector<std::string> SignManager::SplitDialog(const std::string &text)
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;

    // Coba split pake | dulu
    bool hasPipe = text.find('|') != std::string::npos;
    char delim = hasPipe ? '|' : '\n';

    while (std::getline(ss, line, delim))
    {
        // Trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r");
        size_t end = line.find_last_not_of(" \t\r");
        if (start != std::string::npos && end != std::string::npos)
            lines.push_back(line.substr(start, end - start + 1));
        else if (start != std::string::npos)
            lines.push_back(line.substr(start));
    }

    return lines;
}

/**
 * @brief Spawn semua sign dari object layer Tiled
 *
 * Baca custom property "dialog" dari tiap object, split jadi baris,
 * snap posisi ke tile grid, dan daftarkan ke DynamicObstacles.
 *
 * @param signObjects Daftar pointer MapObject bertipe sign
 */
void SignManager::SpawnSigns(const std::vector<MapObject *> &signObjects)
{
    signs.clear();
    for (auto *obj : signObjects)
    {
        Vector2 snapped = SnapToTileGrid({obj->bounds.x, obj->bounds.y});

        SignData d;
        d.tile.name = obj->name;
        d.tile.bounds = obj->bounds;
        d.tile.position = snapped;
        d.tile.state = ObjectState::Closed;

        // Baca dialog dari custom property Tiled, split jadi baris
        auto it = obj->properties.find("dialog");
        if (it != obj->properties.end())
            d.lines = SplitDialog(it->second.getValue<std::string>());
        else
            d.lines = {"(tidak ada teks)"};

        signs.push_back(d);
        DynamicObstacles.push_back(d.tile.bounds);

        TraceLog(LOG_INFO, "SIGN: spawned '%s' at (%.1f, %.1f) — %d baris",
                 obj->name.c_str(), snapped.x, snapped.y, (int)d.lines.size());
    }
}

/**
 * @brief Cari sign terdekat dari titik hit
 * @param hitPos Posisi hit dari player
 * @param threshold Toleransi jarak ke tepi bounds
 * @return Pointer ke sign terdekat, nullptr jika tidak ada
 */
SignManager::SignData *SignManager::FindSign(Vector2 hitPos, float threshold)
{
    SignData *closest = nullptr;
    float minDist = threshold;

    for (auto &sign : signs)
    {
        if (IsHitInBounds(hitPos, sign.tile.bounds, threshold))
        {
            float dist = DistToCenter(hitPos, sign.tile.bounds);
            if (dist < minDist)
            {
                minDist = dist;
                closest = &sign;
            }
        }
    }
    return closest;
}

/**
 * @brief Trigger interaksi player dengan sign
 *
 * Cari sign di sekitar hitPos, set active dialog state,
 * lalu TraceLog tiap baris dialog.
 *
 * @param hitPos Posisi interaksi player
 */
void SignManager::Interact(Vector2 hitPos)
{
    if (isDialogActive)
    {
        DismissDialog();
        return;
    }

    SignData *sign = FindSign(hitPos);
    if (!sign)
    {
        TraceLog(LOG_WARNING, "SIGN: no sign found at (%.1f, %.1f)", hitPos.x, hitPos.y);
        return;
    }

    isDialogActive = true;
    activeDialogLines = sign->lines;

    TraceLog(LOG_INFO, "SIGN INTERACT: '%s' — %d baris",
             sign->tile.name.c_str(), (int)activeDialogLines.size());
    for (size_t i = 0; i < activeDialogLines.size(); i++)
        TraceLog(LOG_INFO, "  [%d] %s", (int)i, activeDialogLines[i].c_str());
}

/**
 * @brief Tutup dialog sign
 */
void SignManager::DismissDialog()
{
    isDialogActive = false;
    activeDialogLines.clear();
    TraceLog(LOG_INFO, "SIGN: dialog ditutup");
}

/**
 * @brief Render placeholder sign
 *
 * Render sebagai DARKGREEN rectangle. Belum pakai sprite.
 *
 * @param viewRect Bounding box area visible
 * @return Jumlah sign yang di-render
 */
int SignManager::Render(Rectangle viewRect)
{
    int rendered = 0;
    for (auto &s : signs)
    {
        if (!CheckCollisionRecs(s.tile.bounds, viewRect))
            continue;

        Display display;
        display.position = s.tile.position;
        DrawFrame("sign", display);
        rendered++;
    }
    return rendered;
}

/**
 * @brief Bersihkan semua data sign
 */
void SignManager::Clear()
{
    signs.clear();
    DismissDialog();
}
