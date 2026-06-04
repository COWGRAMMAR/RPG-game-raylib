/**
 * @file worldgenenartion.cpp
 * @brief Implementasi dari World Generation Core Module
 *
 * File ini berisi implementasi untuk:
 * - GridMath: utility functions untuk manipulasi grid 2D
 * - Internal helpers: PreFabLoadMap, rotasi, dan manajemen memory prefab
 * - WorldGenPools: load/unload room dan corridor pool
 * - WorldGenPrefab: load/unload/rotate prefab individual
 * - WorldGenCanvas: stamping prefab dan corridor ke main map
 */

#include "worldgenenartion.h"
#include "mapLogic.h"
#include <filesystem>
#include <random>
#include <vector>

extern TilesonMapData *tilesonMap;

/*==============================================================================
 * GridMath
 *==============================================================================*/

namespace GridMath
{
    /** @brief Ubah data tile flat 1D ke grid 2D */
    std::vector<std::vector<int>> ReshapeTo2D(const std::vector<int> &flatData, int width, int height)
    {
        std::vector<std::vector<int>> grid(height, std::vector<int>(width));
        for (int i = 0; i < (int)flatData.size(); i++)
        {
            grid[i / width][i % width] = flatData[i];
        }
        return grid;
    }

    /** @brief Ubah grid 2D kembali ke data tile flat 1D */
    std::vector<int> FlattenTo1D(const std::vector<std::vector<int>> &grid)
    {
        std::vector<int> flat;
        for (const auto &row : grid)
        {
            for (int val : row)
            {
                flat.push_back(val);
            }
        }
        return flat;
    }

    /** @brief Rotasi grid 90 derajat searah jarum jam */
    std::vector<std::vector<int>> Rotate90CW(const std::vector<std::vector<int>> &grid)
    {
        int rows = (int)grid.size();
        int cols = (int)grid[0].size();
        std::vector<std::vector<int>> result(cols, std::vector<int>(rows));
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                result[c][rows - 1 - r] = grid[r][c];
            }
        }
        return result;
    }

    /** @brief Rotasi grid dengan kelipatan 90 derajat */
    std::vector<std::vector<int>> RotateGrid(const std::vector<std::vector<int>> &grid, int degrees)
    {
        int times = (degrees / 90) % 4;
        std::vector<std::vector<int>> result = grid;
        for (int i = 0; i < times; i++)
        {
            result = Rotate90CW(result);
        }
        return result;
    }

    /** @brief Rotasi titik 2D terhadap center */
    Vector2 RotatePoint(Vector2 point, Vector2 center, int degrees)
    {
        float x = point.x - center.x;
        float y = point.y - center.y;

        for (int i = 0; i < (degrees / 90) % 4; i++)
        {
            float temp = x;
            x = -y;
            y = temp;
        }

        return {x + center.x, y + center.y};
    }
}

/*==============================================================================
 * Internal Helpers
 *==============================================================================*/

namespace
{
    // Load map JSON Tiled ke TilesonMapData
    void PreFabLoadMap(const char *mapPath, TilesonMapData *target)
    {
        TraceLog(LOG_INFO, "PreFabLoadMap: loading %s", mapPath);
        tson::Tileson t;
        auto parsed = t.parse(mapPath);

        if (parsed->getStatus() != tson::ParseStatus::OK)
        {
            TraceLog(LOG_ERROR, "Tileson: Failed to parse map: %s", parsed->getStatusMessage().c_str());
            return;
        }

        // Baca ukuran map
        tson::Vector2i mapSize = parsed->getSize();
        target->width = mapSize.x;
        target->height = mapSize.y;

        // Hitung jumlah tile layer
        int layerCount = 0;
        for (auto &layer : parsed->getLayers())
            if (layer.getType() == tson::LayerType::TileLayer)
                layerCount++;

        // Alokasi array tiles
        target->layerCount = layerCount;
        target->tiles = new int *[layerCount];
        for (int i = 0; i < layerCount; i++)
            target->tiles[i] = new int[target->width * target->height]();

        // Copy tile data dari tson ke array
        int layerIndex = 0;
        for (auto &layer : parsed->getLayers())
        {
            if (layer.getType() == tson::LayerType::TileLayer)
            {
                std::map<std::tuple<int, int>, tson::Tile *> tileData = layer.getTileData();
                for (const auto &[pos, tile] : tileData)
                {
                    int x = std::get<0>(pos);
                    int y = std::get<1>(pos);
                    if (tile != nullptr && x < target->width && y < target->height)
                        target->tiles[layerIndex][y * target->width + x] = (int)tile->getGid();
                }
                layerIndex++;
            }
        }

        // Copy object data dari tson
        for (auto &layer : parsed->getLayers())
        {
            if (layer.getType() == tson::LayerType::ObjectGroup)
            {
                for (auto &obj : layer.getObjects())
                {
                    MapObject mapObj;
                    mapObj.id = obj.getId();
                    mapObj.name = obj.getName();
                    mapObj.type = obj.getType();
                    mapObj.layerName = layer.getName();

                    tson::Vector2i pos = obj.getPosition();
                    tson::Vector2i size = obj.getSize();
                    mapObj.bounds = {(float)pos.x, (float)pos.y, (float)size.x, (float)size.y};

                    const std::vector<tson::Vector2i> &polygon = obj.getPolygons();
                    if (!polygon.empty())
                    {
                        mapObj.hasPolygon = true;
                        for (const auto &point : polygon)
                            mapObj.polygonPoints.push_back({(float)pos.x + (float)point.x,
                                                            (float)pos.y + (float)point.y});
                    }

                    for (auto &[key, prop] : obj.getProperties().getProperties())
                        mapObj.properties[key] = prop;

                    target->Objects.push_back(mapObj);
                }
            }
        }

        // Load tileset textures
        auto &tilesetList = parsed->getTilesets();
        for (int i = 0; i < (int)tilesetList.size(); i++)
        {
            tson::Tileset *tileset = &tilesetList[i];
            std::string imagePath = "assets/textures/" + tileset->getImagePath().filename().string();
            // TraceLog(LOG_INFO, "Tileson: Loading tileset: %s", imagePath.c_str());

            TilesetInfo info;

            // Reuse texture yang sudah pernah di-load
            if (textures[TILESET_MAP_1].id != 0 && imagePath == "assets/textures/tiles.png")
            {
                info.texture = textures[TILESET_MAP_1];
                // TraceLog(LOG_INFO, "Tileson: Reusing cached texture for tiles.png");
            }
            else if (textures[TILESET_MAP_2].id != 0 && imagePath == "assets/textures/test.png")
            {
                info.texture = textures[TILESET_MAP_2];
                // TraceLog(LOG_INFO, "Tileson: Reusing cached texture for test.png");
            }
            else if (textures[TILESET_PROPS].id != 0 && imagePath == "assets/textures/props.png")
            {
                info.texture = textures[TILESET_PROPS];
                // TraceLog(LOG_INFO, "Tileson: Reusing cached texture for props.png");
            }
            else if (textures[TILESET_ITEMS].id != 0 && imagePath == "assets/textures/items.png")
            {
                info.texture = textures[TILESET_ITEMS];
                // TraceLog(LOG_INFO, "Tileson: Reusing cached texture for items.png");
            }
            else
            {
                // Load texture baru dari file
                Image img = LoadImage(imagePath.c_str());
                info.texture = LoadTextureFromImage(img);
                UnloadImage(img);
            }

            info.cols = tileset->getColumns();
            info.spacing = tileset->getSpacing();
            info.firstgid = tileset->getFirstgid();
            info.lastgid = (i + 1 < (int)tilesetList.size())
                               ? tilesetList[i + 1].getFirstgid() - 1
                               : INT_MAX;

            if (target->tilesets.empty())
                target->tilesets.push_back({});
            target->tilesets[0].push_back(info);
            target->layerTilesetGroup.assign(target->layerCount, 0);
            // TraceLog(LOG_INFO, "Tileson: Loaded tileset %s (gid %d-%d)", imagePath.c_str(), info.firstgid, info.lastgid);
        }

        TraceLog(LOG_INFO, "PreFabLoadMap: done %s", mapPath);
    }

    // Unload prefab dan bebaskan memory
    void UnloadPrefab(TilesonMapData *prefab)
    {
        if (!prefab)
            return;

        if (prefab->tiles)
        {
            for (int i = 0; i < prefab->layerCount; i++)
            {
                delete[] prefab->tiles[i];
            }
            delete[] prefab->tiles;
        }
        delete prefab;
    }

    // Rotasi tile layer prefab
    TilesonMapData *RotateTileLayersInternal(TilesonMapData *source, int degrees)
    {
        TilesonMapData *result = new TilesonMapData();
        result->width = source->width;
        result->height = source->height;
        result->layerCount = source->layerCount;
        result->tilesets = source->tilesets;
        result->layerTilesetGroup = source->layerTilesetGroup;

        result->tiles = new int *[result->layerCount];
        for (int i = 0; i < result->layerCount; i++)
        {
            result->tiles[i] = new int[result->width * result->height]();

            // Flat → 2D → rotate → flatten kembali
            std::vector<int> flat(source->tiles[i], source->tiles[i] + source->width * source->height);
            auto grid2D = GridMath::ReshapeTo2D(flat, source->width, source->height);
            auto rotated = GridMath::RotateGrid(grid2D, degrees);
            auto flatResult = GridMath::FlattenTo1D(rotated);

            for (int j = 0; j < (int)flatResult.size(); j++)
                result->tiles[i][j] = flatResult[j];
        }

        return result;
    }

    // Rotasi object layer prefab
    void RotateObjectLayerInternal(TilesonMapData *source, TilesonMapData *result, int degrees, int tileSize, bool obstacleOnly)
    {
        Vector2 center = {(source->width * tileSize) / 2.0f, (source->height * tileSize) / 2.0f};

        for (auto &obj : source->Objects)
        {
            // Filter berdasarkan tipe
            bool isObstacle = obj.layerName == COLLISION_LAYER_NAME;
            bool isExit = obj.layerName == EXIT_LAYER_NAME;
            if (obstacleOnly && !isObstacle && !isExit)
                continue;
            if (!obstacleOnly && (isObstacle || isExit))
                continue;

            MapObject rotated = obj;

            // Hitung bounding box baru setelah rotasi
            Vector2 topLeft = GridMath::RotatePoint({obj.bounds.x, obj.bounds.y}, center, degrees);
            Vector2 topRight = GridMath::RotatePoint({obj.bounds.x + obj.bounds.width, obj.bounds.y}, center, degrees);
            Vector2 bottomLeft = GridMath::RotatePoint({obj.bounds.x, obj.bounds.y + obj.bounds.height}, center, degrees);
            Vector2 bottomRight = GridMath::RotatePoint({obj.bounds.x + obj.bounds.width, obj.bounds.y + obj.bounds.height}, center, degrees);

            float minX = std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            float minY = std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
            float maxX = std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            float maxY = std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});

            rotated.bounds = {minX, minY, maxX - minX, maxY - minY};

            // Rotasi polygon points
            if (obj.hasPolygon)
            {
                rotated.polygonPoints.clear();
                for (auto &point : obj.polygonPoints)
                {
                    rotated.polygonPoints.push_back(GridMath::RotatePoint(point, center, degrees));
                }
            }

            // Remap nama object exit sesuai arah rotasi
            if (isExit)
            {
                static const std::map<std::string, std::string> exitRemap90CW = {
                    {"exit_north", "exit_east"},
                    {"exit_east", "exit_south"},
                    {"exit_south", "exit_west"},
                    {"exit_west", "exit_north"}};

                int times = (degrees / 90) % 4;
                std::string remappedName = rotated.name;
                for (int i = 0; i < times; i++)
                {
                    auto it = exitRemap90CW.find(remappedName);
                    if (it != exitRemap90CW.end())
                        remappedName = it->second;
                }
                rotated.name = remappedName;
                rotated.type = remappedName;
            }

            result->Objects.push_back(rotated);
        }
    }

    // Rotasi penuh prefab (tile + object layer)
    TilesonMapData *RotatePrefabInternal(TilesonMapData *source, int tileDegrees, int objectDegrees)
    {
        TilesonMapData *result = RotateTileLayersInternal(source, tileDegrees);
        RotateObjectLayerInternal(source, result, tileDegrees, FRAME_SIZE, true);
        RotateObjectLayerInternal(source, result, objectDegrees, FRAME_SIZE, false);
        return result;
    }
}

/*==============================================================================
 * WorldGenPools
 *==============================================================================*/

/** @brief Constructor — init pool dengan seed */
WorldGenPools::WorldGenPools(uint64_t seed)
    : worldSeed(seed)
{
}

/**
 * @brief Hash SplitMix64 untuk deterministic selection
 *
 * Algoritma SplitMix64 sederhana yang dipakai untuk ngasilin index
 * deterministic dari kombinasi seed, posisi, dan salt exit.
 */
uint64_t WorldGenPools::SplitMix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
    return x ^ (x >> 31);
}

/**
 * @brief Pilih index corridor secara deterministic
 *
 * Menggabungkan worldSeed, posisi tile, dan salt berdasarkan arah exit
 * untuk menghasilkan index yang konsisten (sama setiap kali dijalankan).
 */
int WorldGenPools::PickCorridorIndex(int tileX, int tileY, int exitTypeHash, int poolSize)
{
    static const uint64_t EXIT_SALT[4] = {
        0x9e3779b97f4a7c15,
        0xbf58476d1ce4e5b9,
        0x94d049bb133111eb,
        0x1234567890abcdef};

    if (exitTypeHash < 1 || exitTypeHash > 4)
        exitTypeHash = 1;

    uint64_t posHash = SplitMix64((uint64_t)tileX * WG_CANVAS_TILES + (uint64_t)tileY);
    uint64_t combined = worldSeed + posHash + EXIT_SALT[exitTypeHash - 1];
    return (int)(SplitMix64(combined) % (uint64_t)poolSize);
}

/**
 * @brief Ambil corridor prefab deterministic dari pool
 *
 * @note Pool dikirim sebagai reference biar fungsi ini bisa dipakai
 * untuk pool vertical maupun horizontal.
 */
TilesonMapData *WorldGenPools::GetRandomCorridor(WeightedPool &pool, int tileX, int tileY, int exitTypeHash)
{
    if (pool.prefabs.empty())
        return nullptr;
    int idx = PickCorridorIndex(tileX, tileY, exitTypeHash, (int)pool.prefabs.size());
    return pool.prefabs[idx];
}

/**
 * @brief Load semua corridor prefab dari subfolder corridor_v dan corridor_h
 *
 * Setiap prefab di-load, lalu digenerate varian rotasi 180 derajat-nya.
 */
void WorldGenPools::LoadCorridorPool(const char *basePath)
{
    if (corridorPool.loaded)
        return;

    namespace fs = std::filesystem;

    // Load corridor vertical
    std::string vPath = std::string(basePath) + "/corridor_v";
    int vCount = 0;
    {
        std::vector<fs::directory_entry> entries;
        for (auto &entry : fs::directory_iterator(vPath))
            entries.push_back(entry);
        std::sort(entries.begin(), entries.end(),
                  [](auto &a, auto &b)
                  { return a.path() < b.path(); });
        for (auto &entry : entries)
        {
            if (entry.path().extension() != ".json")
                continue;
            TilesonMapData *data = new TilesonMapData();
            PreFabLoadMap(entry.path().string().c_str(), data);
            corridorPool.vertical.prefabs.push_back(data);
            vCount++;
        }
    }
    // Generate varian 180 derajat untuk vertical
    for (int i = 0; i < vCount; i++)
    {
        TilesonMapData *rot = RotatePrefabInternal(corridorPool.vertical.prefabs[i], 180, 180);
        corridorPool.vertical.prefabs.push_back(rot);
    }

    // Load corridor horizontal
    std::string hPath = std::string(basePath) + "/corridor_h";
    int hCount = 0;
    {
        std::vector<fs::directory_entry> entries;
        for (auto &entry : fs::directory_iterator(hPath))
            entries.push_back(entry);
        std::sort(entries.begin(), entries.end(),
                  [](auto &a, auto &b)
                  { return a.path() < b.path(); });
        for (auto &entry : entries)
        {
            if (entry.path().extension() != ".json")
                continue;
            TilesonMapData *data = new TilesonMapData();
            PreFabLoadMap(entry.path().string().c_str(), data);
            corridorPool.horizontal.prefabs.push_back(data);
            hCount++;
        }
    }
    // Generate varian 180 derajat untuk horizontal
    for (int i = 0; i < hCount; i++)
    {
        TilesonMapData *rot = RotatePrefabInternal(corridorPool.horizontal.prefabs[i], 180, 180);
        corridorPool.horizontal.prefabs.push_back(rot);
    }

    corridorPool.loaded = true;
}

/** @brief Unload semua corridor prefab */
void WorldGenPools::UnloadCorridorPool()
{
    for (auto *p : corridorPool.vertical.prefabs)
        UnloadPrefab(p);
    for (auto *p : corridorPool.horizontal.prefabs)
        UnloadPrefab(p);
    corridorPool.vertical.prefabs.clear();
    corridorPool.horizontal.prefabs.clear();
    corridorPool.loaded = false;
}

/** @brief Muat room prefab untuk room type tertentu (start/normal/boss) */
void WorldGenPools::LoadRoomPoolForType(const char *basePath, const char *roomType, RoomPool &targetPool)
{
    namespace fs = std::filesystem;

    struct
    {
        const char *folder;
        WeightedPool &pool;
    } configs[] = {
        {"template_u", targetPool.u},
        {"template_ud", targetPool.ud},
        {"template_ur", targetPool.ur},
        {"template_udr", targetPool.udr},
        {"template_udrl", targetPool.udrl},
    };

    for (auto &[folder, pool] : configs)
    {
        std::string path = std::string(basePath) + "/rooms/" + roomType + "/" + folder;
        if (!fs::exists(path))
            continue;
        {
            std::vector<fs::directory_entry> entries;
            for (auto &entry : fs::directory_iterator(path))
                entries.push_back(entry);
            std::sort(entries.begin(), entries.end(),
                      [](auto &a, auto &b)
                      { return a.path() < b.path(); });
            for (auto &entry : entries)
            {
                if (entry.path().extension() != ".json")
                    continue;
                TilesonMapData *data = new TilesonMapData();
                PreFabLoadMap(entry.path().string().c_str(), data);
                pool.prefabs.push_back(data);
            }
        }
    }

    // Fallback: beberapa type (start/finish/trader/boss) file-nya flat langsung di /rooms/{type}/
    // tanpa subfolder template_u. Muat file-file itu ke pool u (1-exit).
    std::string flatPath = std::string(basePath) + "/rooms/" + roomType;
    if (fs::exists(flatPath))
    {
        std::vector<fs::directory_entry> flatEntries;
        for (auto &entry : fs::directory_iterator(flatPath))
            flatEntries.push_back(entry);
        std::sort(flatEntries.begin(), flatEntries.end(),
                  [](auto &a, auto &b)
                  { return a.path() < b.path(); });
        for (auto &entry : flatEntries)
        {
            if (entry.path().extension() != ".json")
                continue;
            if (entry.is_directory())
                continue;
            TilesonMapData *data = new TilesonMapData();
            PreFabLoadMap(entry.path().string().c_str(), data);
            targetPool.u.prefabs.push_back(data);
        }
    }
}

/** @brief Muat semua room prefab dari direktori base */
void WorldGenPools::LoadRoomPool(const char *basePath)
{
    if (enemyPool.loaded)
        return;

    LoadRoomPoolForType(basePath, "enemy", enemyPool);
    LoadRoomPoolForType(basePath, "treasure", treasurePool);
    LoadRoomPoolForType(basePath, "elite", elitePool);
    LoadRoomPoolForType(basePath, "trader", traderPool);
    LoadRoomPoolForType(basePath, "start", startPool);
    LoadRoomPoolForType(basePath, "finish", finishPool);
    LoadRoomPoolForType(basePath, "boss", bossPool);

    enemyPool.loaded = true;
    treasurePool.loaded = true;
    elitePool.loaded = true;
    traderPool.loaded = true;
    startPool.loaded = true;
    finishPool.loaded = true;
    bossPool.loaded = true;
}

static void UnloadSingleRoomPool(RoomPool &pool)
{
    for (auto *p : pool.u.prefabs)
        UnloadPrefab(p);
    for (auto *p : pool.ud.prefabs)
        UnloadPrefab(p);
    for (auto *p : pool.ur.prefabs)
        UnloadPrefab(p);
    for (auto *p : pool.udr.prefabs)
        UnloadPrefab(p);
    for (auto *p : pool.udrl.prefabs)
        UnloadPrefab(p);
    pool.u.prefabs.clear();
    pool.ud.prefabs.clear();
    pool.ur.prefabs.clear();
    pool.udr.prefabs.clear();
    pool.udrl.prefabs.clear();
    pool.loaded = false;
}

/** @brief Unload semua room prefab yang sudah di-load */
void WorldGenPools::UnloadRoomPool()
{
    UnloadSingleRoomPool(enemyPool);
    UnloadSingleRoomPool(treasurePool);
    UnloadSingleRoomPool(elitePool);
    UnloadSingleRoomPool(traderPool);
    UnloadSingleRoomPool(startPool);
    UnloadSingleRoomPool(finishPool);
    UnloadSingleRoomPool(bossPool);
}

/** @brief Dapatkan room pool berdasarkan cell type */
RoomPool &WorldGenPools::GetPoolForType(CellType type)
{
    switch (type)
    {
    case CELL_ENEMY:
        return enemyPool;
    case CELL_TREASURE:
        return treasurePool;
    case CELL_ENEMY_ELITE:
        return elitePool;
    case CELL_TRADER:
        return traderPool;
    case CELL_START:
        return startPool;
    case CELL_FINISH:
        return finishPool;
    case CELL_BOSS:
        return bossPool;
    default:
        return enemyPool;
    }
}

/** @brief Ambil semua room prefab dari seluruh pool */
std::vector<TilesonMapData *> WorldGenPools::GetAllRoomPrefabs()
{
    std::vector<TilesonMapData *> all;
    auto collect = [&](RoomPool &pool)
    {
        for (auto *p : pool.u.prefabs)
            all.push_back(p);
        for (auto *p : pool.ud.prefabs)
            all.push_back(p);
        for (auto *p : pool.ur.prefabs)
            all.push_back(p);
        for (auto *p : pool.udr.prefabs)
            all.push_back(p);
        for (auto *p : pool.udrl.prefabs)
            all.push_back(p);
    };
    collect(enemyPool);
    collect(treasurePool);
    collect(elitePool);
    collect(traderPool);
    collect(startPool);
    collect(finishPool);
    collect(bossPool);
    return all;
}

/** @brief Dapatkan corridor pool */
CorridorPool &WorldGenPools::GetCorridorPool()
{
    return corridorPool;
}

/*==============================================================================
 * WorldGenPrefab
 *==============================================================================*/

/** @brief Constructor default WorldGenPrefab */
WorldGenPrefab::WorldGenPrefab()
    : data(nullptr)
{
}

/** @brief Load prefab dari file JSON */
void WorldGenPrefab::Load(const char *mapPath)
{
    data = new TilesonMapData();
    PreFabLoadMap(mapPath, data);
}

/** @brief Unload prefab */
void WorldGenPrefab::Unload()
{
    if (!data)
        return;
    UnloadPrefab(data);
    data = nullptr;
}

/** @brief Dapatkan data tileson dari prefab */
TilesonMapData *WorldGenPrefab::GetData() const
{
    return data;
}

/**
 * @brief Buat prefab baru hasil rotasi
 *
 @param tileDegrees Rotasi tile layer
 @param objectDegrees Rotasi object layer non-obstacle
 @return WorldGenPrefab baru — ownership data terpisah dari source
*/
WorldGenPrefab WorldGenPrefab::Rotate(int tileDegrees, int objectDegrees)
{
    WorldGenPrefab result;
    result.data = RotatePrefabInternal(data, tileDegrees, objectDegrees);
    return result;
}

/*==============================================================================
 * WorldGenCanvas
 *==============================================================================*/

/** @brief Constructor — init canvas untuk worldgen */
WorldGenCanvas::WorldGenCanvas(TilesonMapData *map, WorldGenPools *pools, uint64_t seed)
    : tilesonMap(map), pools(pools), wgRng(seed)
{
}

/** @brief Dapatkan slot object (slot_worldgen) untuk stamping */
std::vector<MapObject> WorldGenCanvas::GetSlots()
{
    const auto &slots = TilesonGetObjectsByType(SLOT_WORLDGEN_LAYER_NAME);
    TraceLog(LOG_INFO, "WorldGen: slots found = %d", (int)slots.size());
    std::vector<MapObject> result;
    for (auto *ptr : slots)
        result.push_back(*ptr);

    std::sort(result.begin(), result.end(), [](const MapObject &a, const MapObject &b)
              {
        int idxA = 0, idxB = 0;
        if (a.name.rfind("slot_", 0) == 0)
            idxA = std::stoi(a.name.substr(5));
        if (b.name.rfind("slot_", 0) == 0)
            idxB = std::stoi(b.name.substr(5));
        return idxA < idxB; });

    return result;
}

/** @brief Expand map layers untuk accommodate worldgen layout */
void WorldGenCanvas::ExpandLayers(int extraLayers)
{
    int newLayerCount = tilesonMap->layerCount + extraLayers;

    int **newTiles = new int *[newLayerCount];

    // Copy pointer layer lama
    for (int i = 0; i < tilesonMap->layerCount; i++)
        newTiles[i] = tilesonMap->tiles[i];

    // Alokasi layer baru (diisi 0)
    for (int i = tilesonMap->layerCount; i < newLayerCount; i++)
        newTiles[i] = new int[tilesonMap->width * tilesonMap->height]();

    delete[] tilesonMap->tiles;
    tilesonMap->tiles = newTiles;
    tilesonMap->layerCount = newLayerCount;

    tilesonMap->layerTilesetGroup.resize(newLayerCount, 0);
}

/**
 * @brief Stamp source map ke canvas
 *
 * Proses:
 * 1. Cek apakah tileset source sudah ada di canvas — reuse atau append
 * 2. Copy tile data per layer
 * 3. Copy object data dengan offset posisi
 */
void WorldGenCanvas::Stamp(TilesonMapData *source, int offsetX, int offsetY, int targetLayerOffset)
{
    // Cari tileset group yang sama di canvas
    int newGroupIdx = -1;
    for (int i = 0; i < (int)tilesonMap->tilesets.size(); i++)
    {
        if (tilesonMap->tilesets[i][0].texture.id == source->tilesets[0][0].texture.id)
        {
            newGroupIdx = i;
            break;
        }
    }
    // Kalo gak ketemu, append tileset group baru
    if (newGroupIdx == -1)
    {
        newGroupIdx = (int)tilesonMap->tilesets.size();
        tilesonMap->tilesets.push_back(source->tilesets[0]);
    }

    // Stamp tile data
    for (int l = 0; l < source->layerCount; l++)
    {
        int dstLayer = targetLayerOffset + l;
        if (dstLayer >= tilesonMap->layerCount)
            break;

        tilesonMap->layerTilesetGroup[dstLayer] = newGroupIdx;

        for (int y = 0; y < source->height; y++)
        {
            for (int x = 0; x < source->width; x++)
            {
                int srcTile = source->tiles[l][y * source->width + x];
                if (srcTile == 0)
                    continue;

                int cx = offsetX + x;
                int cy = offsetY + y;
                if (cx >= tilesonMap->width || cy >= tilesonMap->height)
                    continue;

                tilesonMap->tiles[dstLayer][cy * tilesonMap->width + cx] = srcTile;
            }
        }
    }

    // Stamp object data dengan offset
    for (auto &obj : source->Objects)
    {
        MapObject stamped = obj;
        stamped.bounds.x += offsetX * WG_TILE_SIZE;
        stamped.bounds.y += offsetY * WG_TILE_SIZE;
        for (auto &p : stamped.polygonPoints)
        {
            p.x += offsetX * WG_TILE_SIZE;
            p.y += offsetY * WG_TILE_SIZE;
        }
        tilesonMap->Objects.push_back(stamped);
    }
}

/**
 * @brief Stamp prefab ke tengah slot worldgen
 *
 * Menghitung offset agar prefab terletak di tengah slot,
 * lalu memanggil Stamp dengan layer offset WG_PREFAB_LAYER_START.
 */
void WorldGenCanvas::StampToSlot(TilesonMapData *prefab, MapObject *slot)
{
    int slotTileX = (int)(slot->bounds.x / WG_TILE_SIZE);
    int slotTileY = (int)(slot->bounds.y / WG_TILE_SIZE);
    int slotTileW = (int)(slot->bounds.width / WG_TILE_SIZE);
    int slotTileH = (int)(slot->bounds.height / WG_TILE_SIZE);

    // Hitung offset agar prefab ke tengah
    int offsetX = slotTileX + (slotTileW - prefab->width) / 2;
    int offsetY = slotTileY + (slotTileH - prefab->height) / 2;

    // Expand layer kalo perlu
    if (tilesonMap->layerCount < WG_PREFAB_LAYER_START + prefab->layerCount)
        ExpandLayers((WG_PREFAB_LAYER_START + prefab->layerCount) - tilesonMap->layerCount);

    Stamp(prefab, offsetX, offsetY, WG_PREFAB_LAYER_START);
}

/**
 * @brief Stamp corridor dari exit object ke border slot
 *
 * Deteksi arah exit, lalu stamp corridor vertical/horizontal
 * berulang dari posisi exit sampai ke border cell.
 */
void WorldGenCanvas::StampCorridor(const MapObject &exitObj, int slotCol, int slotRow)
{
    int exitTileX = (int)(exitObj.bounds.x / WG_TILE_SIZE);
    int exitTileY = (int)(exitObj.bounds.y / WG_TILE_SIZE);

    // Konversi tipe exit ke hash (1-4)
    int exitTypeHash = 0;
    if (exitObj.type == "exit_north")
        exitTypeHash = 1;
    else if (exitObj.type == "exit_south")
        exitTypeHash = 2;
    else if (exitObj.type == "exit_east")
        exitTypeHash = 3;
    else if (exitObj.type == "exit_west")
        exitTypeHash = 4;

    // Ambil corridor prefab dari pool — cuma 1 yang dipake sesuai arah exit
    CorridorPool &cp = pools->GetCorridorPool();
    bool isHorizontal = (exitObj.type == "exit_east" || exitObj.type == "exit_west");
    TilesonMapData *corridor = pools->GetRandomCorridor(
        isHorizontal ? cp.horizontal : cp.vertical,
        exitTileX, exitTileY, exitTypeHash);
    if (!corridor)
        return;

    // Expand layer kalo perlu
    if (tilesonMap->layerCount < WG_CORRIDOR_LAYER_START + CORRIDOR_LAYER_COUNT)
        ExpandLayers((WG_CORRIDOR_LAYER_START + CORRIDOR_LAYER_COUNT) - tilesonMap->layerCount);

    // Stamp corridor sesuai arah exit
    if (exitObj.type == "exit_north")
    {
        int border = slotRow * WG_CELL_TILES;
        for (int i = 1; i <= exitTileY - border; i++)
            Stamp(corridor, exitTileX - CORRIDOR_CENTER_OFFSET, exitTileY - i, WG_CORRIDOR_LAYER_START);
    }
    else if (exitObj.type == "exit_south")
    {
        int border = slotRow * WG_CELL_TILES + WG_CELL_TILES - 1;
        for (int i = 1; i <= border - exitTileY; i++)
            Stamp(corridor, exitTileX - CORRIDOR_CENTER_OFFSET, exitTileY + i, WG_CORRIDOR_LAYER_START);
    }
    else if (exitObj.type == "exit_east")
    {
        int border = slotCol * WG_CELL_TILES + WG_CELL_TILES - 1;
        for (int i = 1; i <= border - exitTileX; i++)
            Stamp(corridor, exitTileX + i, exitTileY - CORRIDOR_CENTER_OFFSET, WG_CORRIDOR_LAYER_START);
    }
    else if (exitObj.type == "exit_west")
    {
        int border = slotCol * WG_CELL_TILES;
        for (int i = 1; i <= exitTileX - border; i++)
            Stamp(corridor, exitTileX - i, exitTileY - CORRIDOR_CENTER_OFFSET, WG_CORRIDOR_LAYER_START);
    }
}

/** @brief Stamp seluruh room prefab ke canvas sesuai grid layout */
void WorldGenCanvas::StampLayout(const std::vector<std::vector<WorldCell>> &grid,
                                 const std::vector<MapObject> &slots)
{
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            const WorldCell &cell = grid[r][c];
            if (cell.type == CELL_EMPTY)
                continue;

            int slotIndex = r * WG_GRID_SIZE + c;
            if (slotIndex >= (int)slots.size())
                continue;

            MapObject slot = slots[slotIndex];

            WorldGenPrefab result = ResolveRotation(cell.type, cell.exitMask, wgRng);
            if (result.GetData())
            {
                StampToSlot(result.GetData(), &slot);
                result.Unload();
            }
        }
    }
}
