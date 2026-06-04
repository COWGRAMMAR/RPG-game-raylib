/**
 * @file enemy_ai.cpp
 * @brief Implementasi Enemy AI System
 *
 * File ini berisi implementasi sistem AI untuk enemy:
 * - FlowField: Dijkstra-based flow field untuk chase & return pathfinding
 * - EnemySteering: steering behavior dengan obstacle avoidance & anti-flip
 * - SpatialHash & Separation: spatial partitioning untuk separation force
 * - SpawnFlowFields: flow field per spawn point untuk return behavior
 */

#include "enemy_ai.h"
#include "screen.h"
#include "enemy.h"
#include "map.h"
#include "mapLogic.h"
#include "player.h"
#include "../lib/raylib/include/raymath.h"
#include <queue>
#include <cfloat>
#include <limits>
#include <tuple>

/*==============================================================================
 * Globals
 *==============================================================================*/

/** @brief Instance global flow field chase */
FlowField globalFlowField;
/** @brief Map flow field return per spawn ID */
std::unordered_map<int, SpawnFlowFieldEntry> spawnFlowFields;
/** @brief Antrian rebuild spawn flow field */
std::queue<int> spawnFlowFieldRebuildQueue;
/** @brief Cache obstacle untuk raycast */
std::vector<MapObject> cachedObstacleList;
/** @brief Spatial hash untuk separation */
static SpatialHash g_spatialHash;

/*==============================================================================
 * FlowField
 *==============================================================================*/

/**
 * @brief Tandai flow field tidak valid dan reset state rebuild.
 */
void FlowField::Invalidate()
{
    isReady_ = false;
    lastGoalTile_ = {-1, -1};
    rebuildCooldown_ = 0.f;
}

/**
 * @brief Update flow field player jika goal berpindah tile dan cooldown rebuild sudah selesai.
 * @param playerWorld Posisi player dalam world space
 * @param mapWidth Lebar map dalam satuan tile
 * @param mapHeight Tinggi map dalam satuan tile
 * @note Rebuild di-throttle oleh FLOW_FIELD_REBUILD_COOLDOWN.
 */
void FlowField::Update(Vector2 playerWorld, int mapWidth, int mapHeight)
{
    // throttle — jangan rebuild lebih dari sekali per FLOW_FIELD_REBUILD_COOLDOWN detik
    if (rebuildCooldown_ > 0.f)
    {
        rebuildCooldown_ -= Time::DELTA_TIME;
        return;
    }

    // cek apakah player pindah tile sejak build terakhir
    Vector2 currentTile = {
        floorf(playerWorld.x / FLOW_FIELD_TILE_SIZE),
        floorf(playerWorld.y / FLOW_FIELD_TILE_SIZE)};

    if (currentTile.x == lastGoalTile_.x && currentTile.y == lastGoalTile_.y)
        return;

    Build(playerWorld, mapWidth, mapHeight, FLOW_FIELD_PLAYER_RADIUS);
    rebuildCooldown_ = FLOW_FIELD_REBUILD_COOLDOWN;
}

/**
 * @brief Bangun flow field dari goal dalam radius aktif tertentu.
 * @param goalWorld Posisi goal dalam world space
 * @param mapWidth Lebar map dalam satuan tile
 * @param mapHeight Tinggi map dalam satuan tile
 * @param radius Radius area aktif dari goal dalam satuan tile
 * @note Hanya tile dalam radius aktif yang ditandai walkable dan dihitung arahnya.
 */
void FlowField::Build(Vector2 goalWorld, int mapWidth, int mapHeight, int radius)
{
    // Alokasi grid cuma sekali — hindari 164 heap allocations tiap rebuild
    if (!hasAllocatedGrid_ || gridWidth_ != mapWidth || gridHeight_ != mapHeight)
    {
        gridWidth_ = mapWidth;
        gridHeight_ = mapHeight;
        grid_.assign(gridHeight_, std::vector<Cell>(gridWidth_));
        hasAllocatedGrid_ = true;
    }

    int goalX = Clamp((int)(goalWorld.x / FLOW_FIELD_TILE_SIZE), 0, gridWidth_ - 1);
    int goalY = Clamp((int)(goalWorld.y / FLOW_FIELD_TILE_SIZE), 0, gridHeight_ - 1);

    // hitung batas area aktif
    int startX = std::max(0, goalX - radius);
    int startY = std::max(0, goalY - radius);
    int endX = std::min(gridWidth_ - 1, goalX + radius);
    int endY = std::min(gridHeight_ - 1, goalY + radius);

    // reset cuma area aktif — lebih murah daripada re-assign seluruh 164x164 grid
    for (int y = startY; y <= endY; y++)
        for (int x = startX; x <= endX; x++)
            grid_[y][x] = Cell{};

    // mark walkable hanya dalam area aktif
    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            Vector2 tileCenter = {
                x * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET,
                y * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET};
            grid_[y][x].walkable = IsPositionSafe(tileCenter, 1.f, 1.f, 0.f, 0.f);
            if (grid_[y][x].walkable)
                grid_[y][x].cost = ComputeTileCost(x, y);
        }
    }

    lastGoalTile_ = {(float)goalX, (float)goalY};

    Dijkstra(goalX, goalY, startX, startY, endX, endY);

    isReady_ = true;
}

/**
 * @brief Hitung cost tambahan tile berdasarkan kedekatan dengan obstacle.
 * @param x Koordinat tile X dalam grid
 * @param y Koordinat tile Y dalam grid
 * @return Cost tile, termasuk penalty jika ada obstacle cardinal di sekitarnya
 * @note Satu obstacle cardinal sudah cukup untuk menambah penalty, tidak akumulatif.
 */
float FlowField::ComputeTileCost(int x, int y)
{
    float cost = FLOW_FIELD_CARDINAL_COST;

    // sample 4 arah cardinal sejauh 1 tile
    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    for (int i = 0; i < 4; i++)
    {
        Vector2 samplePos = {
            (x + dx[i]) * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET,
            (y + dy[i]) * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET};
        if (!IsPositionSafe(samplePos, 1.f, 1.f, 0.f, 0.f))
        {
            cost += FLOW_FIELD_OBSTACLE_PENALTY;
            break; // cukup satu tetangga obstacle = kena penalty
        }
    }

    return cost;
}

/**
 * @brief Hitung jarak terpendek dan arah tiap tile menuju goal.
 * @param goalX Koordinat tile X goal
 * @param goalY Koordinat tile Y goal
 * @param startX Batas kiri area aktif
 * @param startY Batas atas area aktif
 * @param endX Batas kanan area aktif
 * @param endY Batas bawah area aktif
 * @note Direction tiap cell diisi dari neighbor dengan distance paling kecil.
 */
void FlowField::Dijkstra(int goalX, int goalY, int startX, int startY, int endX, int endY)
{
    std::vector<std::vector<float>> dist(gridHeight_,
                                         std::vector<float>(gridWidth_, std::numeric_limits<float>::max()));

    // {cost, x, y}
    using T = std::tuple<float, int, int>;
    std::priority_queue<T, std::vector<T>, std::greater<T>> pq;

    dist[goalY][goalX] = 0.f;
    pq.push({0.f, goalX, goalY});

    const int dx[] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int dy[] = {1, -1, 0, 0, 1, 1, -1, -1};
    const float moveCost[] = {
        FLOW_FIELD_CARDINAL_COST, FLOW_FIELD_CARDINAL_COST,
        FLOW_FIELD_CARDINAL_COST, FLOW_FIELD_CARDINAL_COST,
        FLOW_FIELD_DIAGONAL_COST, FLOW_FIELD_DIAGONAL_COST,
        FLOW_FIELD_DIAGONAL_COST, FLOW_FIELD_DIAGONAL_COST};

    while (!pq.empty())
    {
        auto [d, cx, cy] = pq.top();
        pq.pop();

        if (d > dist[cy][cx])
            continue; // stale entry

        for (int i = 0; i < 8; i++)
        {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx < startX || nx > endX || ny < startY || ny > endY)
                continue;
            if (!grid_[ny][nx].walkable)
                continue;

            float newDist = dist[cy][cx] + moveCost[i] + grid_[ny][nx].cost;
            if (newDist < dist[ny][nx])
            {
                dist[ny][nx] = newDist;
                pq.push({newDist, nx, ny});
            }
        }
    }

    // assign direction — sama kayak BFS, cari neighbor dengan dist terkecil
    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            if (!grid_[y][x].walkable || dist[y][x] == std::numeric_limits<float>::max())
            {
                grid_[y][x].reached = false;
                grid_[y][x].direction = {0, 0};
                continue;
            }

            grid_[y][x].reached = true;

            float bestDist = dist[y][x];
            Vector2 bestDir = {0, 0};

            for (int i = 0; i < 8; i++)
            {
                int nx = x + dx[i];
                int ny = y + dy[i];

                if (nx < startX || nx > endX || ny < startY || ny > endY)
                    continue;
                if (dist[ny][nx] == std::numeric_limits<float>::max())
                    continue;

                if (dist[ny][nx] < bestDist)
                {
                    bestDist = dist[ny][nx];
                    bestDir = {(float)dx[i], (float)dy[i]};
                }
            }

            grid_[y][x].direction = Vector2Normalize(bestDir);
        }
    }
}

/**
 * @brief Ambil arah flow field untuk posisi world space tertentu.
 * @param worldPos Posisi dalam world space yang akan dicek
 * @return Arah normalized menuju goal, atau {0,0} jika belum siap/tidak valid/tidak reachable
 */
Vector2 FlowField::GetDirection(Vector2 worldPos) const
{
    if (!isReady_)
        return {0, 0};

    int x = (int)(worldPos.x / FLOW_FIELD_TILE_SIZE);
    int y = (int)(worldPos.y / FLOW_FIELD_TILE_SIZE);

    if (!IsValidTile(x, y))
        return {0, 0};
    if (!grid_[y][x].reached)
        return {0, 0};

    return grid_[y][x].direction;
}

/**
 * @brief Cek apakah koordinat tile berada di dalam batas grid flow field.
 * @param x Koordinat tile X
 * @param y Koordinat tile Y
 * @return True jika tile valid
 */
bool FlowField::IsValidTile(int x, int y) const
{
    return x >= 0 && x < gridWidth_ && y >= 0 && y < gridHeight_;
}

/**
 * @brief Ambil cost traversal tile pada posisi world space tertentu.
 * @param worldPos Posisi dalam world space yang akan dicek
 * @return Cost tile, atau FLT_MAX jika tile tidak valid atau tidak reachable
 */
float FlowField::GetCost(Vector2 worldPos) const
{
    int x = (int)(worldPos.x / FLOW_FIELD_TILE_SIZE);
    int y = (int)(worldPos.y / FLOW_FIELD_TILE_SIZE);
    if (!IsValidTile(x, y))
        return FLT_MAX;
    if (!grid_[y][x].reached)
        return FLT_MAX;
    return grid_[y][x].cost;
}

/*==============================================================================
 * Obstacle Cache
 *==============================================================================*/

/**
 * @brief Bangun daftar obstacle dari collision layer dan object dinamis map.
 * @return Daftar MapObject yang bisa dipakai untuk raycast
 * @note Mengambil collision rect/polygon dari Tiled serta object chest dan bomb.
 */
std::vector<MapObject> BuildObstacleList()
{
    std::vector<MapObject> result;

    TiledHelper::CollisionResult col;
    if (TiledHelperFunction.TryGetCollisionByLayerName(COLLISION_LAYER_NAME, col))
    {
        for (auto &rect : col.rects)
        {
            MapObject obj;
            obj.bounds = rect;
            obj.hasPolygon = false;
            result.push_back(obj);
        }
        for (auto &poly : col.polygons)
        {
            MapObject obj;
            obj.polygonPoints = poly;
            obj.hasPolygon = true;
            result.push_back(obj);
        }
    }

    auto appendType = [&](const char *type)
    {
        for (auto *p : TilesonGetObjectsByType(type))
            if (p)
                result.push_back(*p);
    };

    // jika ada object layer yang perlu dipertimbangkan jadi obstacle layer masukin kesini
    appendType(CHEST_TYPE_OBJECT_NAME);
    appendType(BOMB_TYPE_OBJECT_NAME);
    appendType(CRATE_TYPE_OBJECT_NAME);
    appendType(BARRIER_TYPE_OBJECT_NAME);
    appendType(BARRIER_BOSS_TYPE_OBJECT_NAME);

    return result;
}

/*==============================================================================
 * EnemySteering
 *==============================================================================*/

/**
 * @brief Pilih arah steering terbaik dari grid kandidat di sekitar enemy.
 * @param ctx Konteks steering enemy saat ini
 * @param flowDir Arah utama dari flow field
 * @param mode Mode steering yang sedang dipakai
 * @return Arah steering terbaik hasil scoring kandidat
 * @note Saat flip terlalu sering, scoring beralih memakai cost flow field.
 */
Vector2 EnemySteering::EvaluateGrid(const SteeringContext &ctx, Vector2 flowDir, SteeringMode mode)
{
    float bestScore = -FLT_MAX;
    Vector2 bestDir = flowDir;

    SteeringTarget = Vector2Add(ctx.Position, Vector2Scale(flowDir, FLOW_FIELD_TILE_SIZE));

    Vector2 currentFlowTile = {
        floorf(ctx.Position.x / FLOW_FIELD_TILE_SIZE),
        floorf(ctx.Position.y / FLOW_FIELD_TILE_SIZE)};

    for (int dy = -STEERING_GRID_RADIUS; dy <= STEERING_GRID_RADIUS; dy++)
    {
        for (int dx = -STEERING_GRID_RADIUS; dx <= STEERING_GRID_RADIUS; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            int tx = (int)currentFlowTile.x + dx;
            int ty = (int)currentFlowTile.y + dy;

            Vector2 tileCenter = {
                (float)tx * FRAME_SIZE + ctx.TileCenterOffset,
                (float)ty * FRAME_SIZE + ctx.TileCenterOffset};

            if (!IsPositionSafe(tileCenter, ctx.HitBoxValue, ctx.HitBoxValue, ctx.OffsetValue, ctx.OffsetValue))
                continue;

            if (dx != 0 && dy != 0)
            {
                Vector2 ortho1 = {(float)(tx - dx) * FRAME_SIZE + ctx.TileCenterOffset,
                                  (float)ty * FRAME_SIZE + ctx.TileCenterOffset};
                Vector2 ortho2 = {(float)tx * FRAME_SIZE + ctx.TileCenterOffset,
                                  (float)(ty - dy) * FRAME_SIZE + ctx.TileCenterOffset};
                if (!IsPositionSafe(ortho1, ctx.HitBoxValue, ctx.HitBoxValue, ctx.OffsetValue, ctx.OffsetValue) ||
                    !IsPositionSafe(ortho2, ctx.HitBoxValue, ctx.HitBoxValue, ctx.OffsetValue, ctx.OffsetValue))
                    continue;
            }

            Vector2 toTile = Vector2Normalize({tileCenter.x - ctx.Position.x,
                                               tileCenter.y - ctx.Position.y});

            if (SteeringFlipCount >= MaxSteeringFlipCount / 2)
            {
                float cost = (mode == STEERING_CHASE)
                                 ? globalFlowField.GetCost(tileCenter)
                                 : ctx.ReturnFlowField->GetCost(tileCenter);

                if (mode == STEERING_CHASE && !CheckCollisionCircleRec(tileCenter, ctx.DetectionRange, ctx.PlayerHitbox))
                    continue;

                float score = -cost;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDir = toTile;
                    SteeringTarget = tileCenter;
                }
                continue;
            }

            float dot = Vector2DotProduct(toTile, flowDir);
            float momentum = Vector2DotProduct(toTile, SteeringDir) * ScoreMultiplier;
            float score = dot + momentum;

            if (score > bestScore)
            {
                bestScore = score;
                bestDir = toTile;
                SteeringTarget = tileCenter;
            }
        }
    }

    return bestDir;
}

/**
 * @brief Catat perubahan arah berlawanan untuk mengurangi steering bolak-balik.
 * @param bestDir Arah steering terbaik yang baru
 * @param prevDir Arah steering sebelumnya
 */
void EnemySteering::ApplyAntiFlip(Vector2 bestDir, Vector2 prevDir)
{
    float dirChange = Vector2DotProduct(bestDir, prevDir);
    if (dirChange < 0.f)
    {
        SteeringFlipCount++;
        SteeringFlipTimer = SteeringFlipTimerWindow;
    }

    SteeringFlipTimer -= Time::DELTA_TIME;
    if (SteeringFlipTimer <= 0.f)
        SteeringFlipCount = 0;
}

/**
 * @brief Hitung arah steering enemy berdasarkan flow field dan obstacle raycast.
 * @param mode Mode steering chase atau return
 * @param ctx Konteks steering enemy saat ini
 * @param ray RayCast yang dipakai untuk deteksi obstacle
 * @return Arah steering yang dipilih
 * @note Steering di-throttle dan hanya dievaluasi ulang saat tile berubah atau cooldown habis.
 */
Vector2 EnemySteering::Compute(SteeringMode mode, const SteeringContext &ctx, RayCast &ray)
{
    Vector2 flowDir;
    if (mode == STEERING_CHASE)
        flowDir = globalFlowField.GetDirection(ctx.Position);
    else if (mode == STEERING_RETURN)
        flowDir = ctx.ReturnFlowField->GetDirection(ctx.Position);
    else
        flowDir = globalFlowField.GetDirection(ctx.Position);

    if (Vector2LengthSqr(flowDir) < 0.001f)
        return flowDir;

    Vector2 currentFlowTile = {
        floorf(ctx.Position.x / FLOW_FIELD_TILE_SIZE),
        floorf(ctx.Position.y / FLOW_FIELD_TILE_SIZE)};

    Vector2 rayDir = (Vector2LengthSqr(ctx.Velocity) > 0.001f)
                         ? Vector2Normalize(ctx.Velocity)
                         : flowDir;

    if (Vector2LengthSqr(SteeringDir) < 0.001f)
        SteeringDir = flowDir;

    ray.Cast(ctx.Position, rayDir, ctx.rayLength, cachedObstacleList);

    SteeringCooldown -= Time::DELTA_TIME;

    bool tileChanged = (currentFlowTile.x != LastFlowTile.x ||
                        currentFlowTile.y != LastFlowTile.y);

    if (!tileChanged && SteeringCooldown > 0.f)
        return SteeringDir;

    LastFlowTile = currentFlowTile;
    SteeringCooldown = SteeringCooldownWindow;

    Vector2 prevDir = SteeringDir;
    Vector2 bestDir = EvaluateGrid(ctx, flowDir, mode);

    SteeringDir = bestDir;
    ApplyAntiFlip(bestDir, prevDir);

    return SteeringDir;
}

/**
 * @brief Cek apakah player berada dalam radius deteksi langsung enemy.
 * @param ctx Konteks steering enemy saat ini
 * @return True jika hitbox player bersinggungan dengan lingkaran deteksi enemy
 */
bool EnemySteering::IsPlayerInRange(const SteeringContext &ctx)
{
    return CheckCollisionCircleRec(ctx.Position, ctx.rayDetectionLength, ctx.PlayerHitbox);
}

/**
 * @brief Cek range deteksi untuk kebutuhan debug.
 * @param enemyCenter Posisi pusat enemy
 * @param playerHitbox Hitbox player
 * @param rayLength Radius deteksi yang diuji
 * @return True jika hitbox player berada dalam radius deteksi
 */
bool EnemySteering::IsInRangeDebug(Vector2 enemyCenter, Rectangle playerHitbox, float rayLength)
{
    return CheckCollisionCircleRec(enemyCenter, rayLength, playerHitbox);
}

/*==============================================================================
 * Spawn Flow Fields
 *==============================================================================*/

/**
 * @brief Bangun flow field return untuk satu spawn object.
 * @param spawnPos Posisi spawn atau pusat area spawn
 * @param objId ID object spawn dari Tiled
 * @param mapWidth Lebar map dalam satuan tile
 * @param mapHeight Tinggi map dalam satuan tile
 * @note Tidak membangun ulang jika objId sudah punya flow field.
 */
void BuildSpawnFlowFields(Vector2 spawnPos, int objId, int mapWidth, int mapHeight)
{
    if (spawnFlowFields.count(objId))
        return;

    spawnFlowFields[objId].spawnPos = spawnPos;
    spawnFlowFields[objId].field.Build(spawnPos, mapWidth, mapHeight, FLOW_FIELD_RETURN_RADIUS);
}

/**
 * @brief Cari flow field spawn terdekat yang bisa dijangkau dari posisi tertentu.
 * @param position Posisi enemy dalam world space
 * @return Pointer ke flow field terbaik, atau nullptr jika tidak ada yang reachable
 */
FlowField *FindNearestSpawnFlowField(Vector2 position)
{
    FlowField *best = nullptr;
    float bestDist = FLT_MAX;

    for (auto &[id, ff] : spawnFlowFields)
    {
        if (!ff.field.IsReady())
            continue;

        // pakai goal tile sebagai proxy jarak
        Vector2 dir = ff.field.GetDirection(position);
        if (Vector2LengthSqr(dir) < 0.001f)
            continue;

        // cari berdasarkan cost — makin kecil makin dekat ke goal
        float cost = ff.field.GetCost(position);
        if (cost < bestDist)
        {
            bestDist = cost;
            best = &ff.field;
        }
    }

    return best; // nullptr kalau semua unreachable
}

/**
 * @brief Tandai flow field spawn di sekitar posisi tertentu agar masuk antrean rebuild.
 * @param position Posisi world space pusat area dirty
 */
void MarkSpawnFlowFieldsDirty(Vector2 position)
{
    for (auto &[id, entry] : spawnFlowFields)
    {
        float dist = Vector2Distance(entry.spawnPos, position);
        if (dist < FLOW_FIELD_RETURN_RADIUS * FLOW_FIELD_TILE_SIZE)
            spawnFlowFieldRebuildQueue.push(id);
    }
}

/**
 * @brief Bangun ulang spatial hash dari daftar enemy aktif.
 * @param enemies Daftar pointer enemy yang akan dimasukkan ke spatial hash
 */
void RebuildObstacleCache()
{
    cachedObstacleList = BuildObstacleList();
}

/*==============================================================================
 * SpatialHash & Separation
 *==============================================================================*/

/**
 * @brief Bangun ulang spatial hash dari daftar enemy aktif.
 * @param enemies Daftar pointer enemy yang akan dimasukkan ke spatial hash
 */
void RebuildSpatialHash(std::vector<Enemy *> &enemies)
{
    g_spatialHash.Clear();
    for (int i = 0; i < (int)enemies.size(); i++)
    {
        if (!enemies[i]->IsActive || !enemies[i]->IsAlive())
            continue;
        g_spatialHash.Insert(i, enemies[i]->Position);
    }
}

/**
 * @brief Hitung gaya separation agar enemy tidak terlalu menumpuk.
 * @param index Index enemy yang sedang dihitung
 * @param enemies Daftar pointer enemy aktif
 * @return Vektor gaya separation yang sudah dibatasi maksimum
 * @note Neighbor diambil dari spatial hash, lalu difilter lagi berdasarkan status dan jarak aktual.
 */
Vector2 CalcSeparationForce(int index, std::vector<Enemy *> &enemies)
{
    Vector2 force = {0, 0};
    auto neighbors = g_spatialHash.Query(enemies[index]->Position);

    for (int other : neighbors)
    {
        if (other == index)
            continue;
        if (!enemies[other]->IsActive || !enemies[other]->IsAlive())
            continue;

        Vector2 diff = {
            enemies[index]->Position.x - enemies[other]->Position.x,
            enemies[index]->Position.y - enemies[other]->Position.y};
        float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);
        if (dist < SEPARATION_RADIUS && dist > 0.0f)
        {
            float scale = (SEPARATION_RADIUS - dist) / SEPARATION_RADIUS;
            force.x += (diff.x / dist) * scale * SEPARATION_STRENGTH;
            force.y += (diff.y / dist) * scale * SEPARATION_STRENGTH;
        }
    }

    float len = sqrtf(force.x * force.x + force.y * force.y);
    if (len > MAX_SEPARATION_FORCE)
    {
        force.x = (force.x / len) * MAX_SEPARATION_FORCE;
        force.y = (force.y / len) * MAX_SEPARATION_FORCE;
    }

    return force;
}