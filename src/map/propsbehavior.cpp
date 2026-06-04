/**
 * @file propsbehavior.cpp
 * @brief Implementasi Props & Trap Behavior System
 *
 * File ini berisi implementasi untuk semua interactable props dan trap di dungeon:
 * - ChestManager: spawn, interaksi, dan loot chest
 * - SpikeManager: spawn, update timer aktif/nonaktif, damage player & enemy
 * - BombManager: spawn, explode, chain reaction, damage player & enemy
 * - CrateManager: spawn, destroy, dan loot crate
 *
 * Semua manager di-spawn via SpawnObject() yang dipanggil saat map di-load.
 */

#include "propsbehavior.h"
#include "item.h"
#include "enemy.h"
#include "enemy_ai.h"
#include "entities.h"
#include "game_debug.h"

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
}

/**
 * @brief Trigger hit attack player ke semua props yang bisa bereaksi terhadap serangan.
 * @param attackHitbox Hitbox serangan player
 * @param playerBounds Bounding box player untuk efek props tertentu
 * @param player Pointer ke player
 */
void HitPropsByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player)
{
    bombManager.HitByAttack(attackHitbox, playerBounds, player);

    crateManager.HitByAttack(attackHitbox);
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
    TraceLog(LOG_INFO, "SpawnChests called, consumed size: %d", (int)consumedPositions.size());
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
    TraceLog(LOG_INFO, "Looking for chest at (%.1f, %.1f), total chests: %d", hitPos.x, hitPos.y, (int)chests.size());
    TileObject *chest = FindChest(hitPos);
    if (!chest || chest->state == ObjectState::Open)
        return;
    chest->state = ObjectState::Open;
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
    // placeholder, rarity system nyusul
    TraceLog(LOG_INFO, "Chest opened at (%.1f, %.1f)", chest.position.x, chest.position.y);

    int jumlahLoot = GetRandomValue(1, 3);

    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    int lootSpread = 60;
    for (int i = 0; i < jumlahLoot; i++)
    {
        // Kasih sedikit offset random biar itemnya mencar di sekitar chest
        Vector2 spawnPos = {
            chest.position.x + (float)GetRandomValue(-lootSpread, lootSpread),
            chest.position.y + (float)GetRandomValue(-lootSpread, lootSpread)};

        itemData.SpawnItemAtLocation(spawnPos, &rng, ITEM_ANY);
    }
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
        // damage player
        if (CheckCollisionRecs(playerBounds, spike.tile.bounds) && globalPlayerDamageCooldown <= 0.0f && player)
        {
            globalPlayerDamageCooldown = SPIKE_DAMAGE_COOLDOWN;
            player->TakeDamage(SPIKE_DAMAGE);
        }

        // damage enemy
        if (globalEnemyDamageCooldown <= 0.0f)
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
void BombManager::HitByAttack(Rectangle attackHitbox, Rectangle playerBounds, Player *player)
{
    for (auto &bomb : bombs)
    {
        if (!bomb.isAlive || bomb.isExploding)
            continue;
        if (!CheckCollisionAgainstRects(attackHitbox, {bomb.tile.bounds}))
            continue;
        Explode(bomb, playerBounds, player);
    }
}

/**
 * @brief Trigger ledakan bomb
 *
 * Urutan proses:
 * 1. Set state Inactive, tandai isExploding & isTriggered
 * 2. Hapus dari DynamicObstacles
 * 3. Damage player jika dalam radius
 * 4. Damage semua enemy aktif dalam radius
 * 5. Chain reaction: trigger Explode pada bomb lain dalam radius yang belum meledak
 *
 * isTriggered dipakai sebagai guard agar tidak terjadi infinite loop
 * pada layout bomb yang saling berdekatan.
 *
 * @param bomb BombData yang akan diledakkan
 * @param playerBounds Bounding box player
 * @param player Pointer ke player untuk apply damage
 */
void BombManager::Explode(BombData &bomb, Rectangle playerBounds, Player *player)
{
    bomb.tile.state = ObjectState::Inactive;
    bomb.isExploding = true;
    bomb.isTriggered = true;
    bomb.explosionTimer = BOMB_EXPLOSION_DURATION;

    consumedPositions.insert(EncodePos(bomb.tile.position));

    // hapus dari dynamic obstacles
    DynamicObstacles.erase(
        std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                       { return r.x == bomb.tile.bounds.x && r.y == bomb.tile.bounds.y; }),
        DynamicObstacles.end());
    MarkSpawnFlowFieldsDirty(bomb.tile.position);
    RebuildObstacleCache();

    Vector2 bombCenter = {
        bomb.tile.position.x + FRAME_SIZE / 2.0f,
        bomb.tile.position.y + FRAME_SIZE / 2.0f
    };

    if (IsInExplosionRadius(bombCenter, playerBounds))
        if (player)
            player->TakeDamage(BOMB_DAMAGE);

    for (auto entity : Entities::GetRegistry())
    {
        Enemy *enemy = dynamic_cast<Enemy *>(entity);
        if (!enemy)
            continue;
        if (!enemy->IsActive || enemy->Health <= 0)
            continue;
        if (IsInExplosionRadius(bombCenter, enemy->GetHitbox()))
            enemy->TakeDamage(BOMB_DAMAGE, {0, 0});
    }

    // chain reaction
    for (auto &other : bombs)
    {
        if (!other.isAlive || other.isExploding || other.isTriggered)
            continue;
        if (IsInExplosionRadius(bombCenter, other.tile.bounds))
            Explode(other, playerBounds, player);
    }

    crateManager.HitByExplosion(bombCenter, this);
}

/**
 * @brief Cek apakah target rectangle berada dalam radius ledakan bomb
 *
 * Menggunakan nearest-point check dari center bomb ke rectangle target,
 * bukan center-to-center, agar akurat untuk target berukuran besar.
 *
 * @param bombPos Posisi center bomb
 * @param target Bounding box target
 * @return true jika jarak nearest point <= BOMB_EXPLOSION_RADIUS
 */
bool BombManager::IsInExplosionRadius(Vector2 bombPos, Rectangle target)
{
    // cek titik terdekat di rectangle target ke pusat bomb
    float nearestX = Clamp(bombPos.x, target.x, target.x + target.width);
    float nearestY = Clamp(bombPos.y, target.y, target.y + target.height);
    float dist = Vector2Distance(bombPos, {nearestX, nearestY});
    return dist <= BOMB_EXPLOSION_RADIUS;
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
                bomb.tile.position.y + FRAME_SIZE / 2.0f
            };
            float progress = (BOMB_EXPLOSION_DURATION - bomb.explosionTimer) / BOMB_EXPLOSION_DURATION;
            Explosion(bombCenter, BOMB_EXPLOSION_RADIUS, progress);
        }
        else
        {
            if (isDebugMode)
            {
                Vector2 bombCenter = {
                    bomb.tile.position.x + FRAME_SIZE / 2.0f,
                    bomb.tile.position.y + FRAME_SIZE / 2.0f
                };
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
void CrateManager::HitByAttack(Rectangle attackHitbox)
{
    for (auto &crate : crates)
    {
        if (!crate.isAlive)
            continue;
        if (!CheckCollisionAgainstRects(attackHitbox, {crate.tile.bounds}))
            continue;
        Destroy(crate);
    }
}

/**
 * @brief Hancurkan crate yang berada dalam radius ledakan bomb.
 * @param bombPos Posisi pusat ledakan bomb
 * @param bomber BombManager yang dipakai untuk cek radius ledakan
 */
void CrateManager::HitByExplosion(Vector2 bombPos, BombManager *bomber)
{
    for (auto &crate : crates)
    {
        if (!crate.isAlive)
            continue;
        if (bomber->IsInExplosionRadius(bombPos, crate.tile.bounds))
            Destroy(crate);
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

    DynamicObstacles.erase(
        std::remove_if(DynamicObstacles.begin(), DynamicObstacles.end(), [&](const Rectangle &r)
                       { return r.x == crate.tile.bounds.x && r.y == crate.tile.bounds.y; }),
        DynamicObstacles.end());
    MarkSpawnFlowFieldsDirty(crate.tile.position);
    RebuildObstacleCache();

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
    int lootSpread = 60;
    Vector2 spawnPos = {
        crate.position.x + (float)GetRandomValue(-lootSpread, lootSpread),
        crate.position.y + (float)GetRandomValue(-lootSpread, lootSpread)};

    itemData.SpawnItemAtLocation(spawnPos, &rng, ITEM_POTION);
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
    if (totalEnemyCount == 0)
    {
        // Di boss map: exclude boss dari hitungan threshold
        totalEnemyCount = 0;
        for (auto *enemy : Entities::GetEnemyRegistry())
        {
            if (isBossMap && enemy->rank == ENEMY_BOSS)
                continue;
            totalEnemyCount++;
        }
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
            if (enemy->IsActive) bossAlive++;
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
                TraceLog(LOG_INFO, "BarrierManager: boss barrier unlocked (boss defeated)");
            }
        }
    }
    else
    {
        /*-------- Normal map: unlock once threshold met --------*/
        if (!cleared && totalEnemyCount > 0 &&
            (float)deadCount / (float)totalEnemyCount >= KILL_THRESHOLD)
            RemoveAllBarriers();
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
    for (auto &b : barriers)
    {
        if (!b.isActive)
            continue;
        if (!CheckCollisionRecs(b.tile.bounds, viewRect))
            continue;

        DrawRectangleRec(b.tile.bounds, ColorAlpha(YELLOW, 0.3f));
        DrawRectangleLinesEx(b.tile.bounds, 2.0f, YELLOW);
        rendered++;
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
        if (!b.isActive) continue;
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
    for (auto &b : barriers)
    {
        if (b.isActive) continue;
        b.isActive = true;
        DynamicObstacles.push_back(b.tile.bounds);
    }
    cleared = false;
    RebuildObstacleCache();
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
    bossStageBounds = {0};
    totalEnemyCount = 0;
    prevDeadCount = 0;
}