/**
 * @file map.cpp
 * @brief Implementasi dari Map System & Tileson Integration Module
 *
 * File ini berisi implementasi sistem map untuk:
 * - Load dan unload map dari file JSON Tiled
 * - Render tile map ke world space
 * - Hitung tile yang visible untuk frustum culling
 * - Handle perpindahan map dan riwayat navigasi
 */

#include "../lib/raylib/include/raymath.h"
#include "entities.h"
#include "mapLogic.h"
#include "worldgenenartion.h"
#include "map.h"
#include "animation.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "mapstack.h"
#include "entities.h"
#include "movement.h"
#include "propsbehavior.h"
#include "enemy_ai.h"
#include <memory>
#include <string>

/*==============================================================================
 * Global Variables
 *==============================================================================*/

/** @brief Pointer ke data map aktif */
TilesonMapData *tilesonMap = nullptr;

/** @brief Hasil parse map */
static std::unique_ptr<tson::Map> parsedMap = nullptr;

/** @brief Camera rendering */
Camera2D camera = {0};

/** @brief Debug: tile render count */
int lastTilesRendered = 0;

/** @brief Debug: visible range */
TileRange currentVisibleRange = {0, 0, 0, 0};

/** Stack riwayat perpindahan map */
static MapSystem::MapStack mapHistoryStack;

/** Path map yang sedang aktif */
static std::string currentMapPath = "";

/** Path map prefabroom */
static std::string preFabMapPath = "";

/*==============================================================================
 * Map Loading & Unloading
 *==============================================================================*/

/**
 * @brief Load map dari file JSON Tiled
 *
 * Proses load mencakup:
 * - Parse file map dengan Tileson
 * - Simpan data tile untuk setiap tile layer
 * - Simpan seluruh object dari object layer
 * - Load texture tileset yang dipakai map
 *
 * @param mapPath Path file map yang akan dimuat
 */
void LoadMap(const char *mapPath)
{
    // Step 1: Parse file JSON dengan Tileson
    tson::Tileson t;
    parsedMap = t.parse(mapPath);

    if (parsedMap->getStatus() != tson::ParseStatus::OK)
    {
        TraceLog(LOG_ERROR, "Tileson: Failed to parse map: %s", parsedMap->getStatusMessage().c_str());
        return;
    }

    // Step 2: Alokasi struktur data map
    tilesonMap = new TilesonMapData();
    tson::Vector2i mapSize = parsedMap->getSize();
    tilesonMap->width = mapSize.x;
    tilesonMap->height = mapSize.y;

    // Hitung jumlah TileLayer dulu sebelum alokasi array
    int layerCount = 0;
    for (auto &layer : parsedMap->getLayers())
        if (layer.getType() == tson::LayerType::TileLayer)
            layerCount++;

    // Alokasi array tile untuk setiap layer
    tilesonMap->layerCount = layerCount;
    tilesonMap->tiles = new int *[layerCount];
    for (int i = 0; i < layerCount; i++)
        tilesonMap->tiles[i] = new int[tilesonMap->width * tilesonMap->height]();

    // Step 3: Isi data tile tiap layer
    int layerIndex = 0;
    for (auto &layer : parsedMap->getLayers())
    {
        if (layer.getType() == tson::LayerType::TileLayer)
        {
            std::map<std::tuple<int, int>, tson::Tile *> tileData = layer.getTileData();
            for (const auto &[pos, tile] : tileData)
            {
                int x = std::get<0>(pos);
                int y = std::get<1>(pos);
                if (tile != nullptr && x < tilesonMap->width && y < tilesonMap->height)
                    tilesonMap->tiles[layerIndex][(y * tilesonMap->width) + x] = (int)tile->getGid();
            }
            layerIndex++;
        }
    }

    // Debug log sample tile dari layer terakhir yang terbaca
    TraceLog(LOG_INFO, "Layer %d: sample tileId[0]=%d tileId[1]=%d",
             layerIndex - 1,
             tilesonMap->tiles[layerIndex - 1][0],
             tilesonMap->tiles[layerIndex - 1][1]);

    // Step 4: Baca semua object dari ObjectGroup layer
    for (auto &layer : parsedMap->getLayers())
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

                // Simpan polygon dalam world space jika object memilikinya
                const std::vector<tson::Vector2i> &polygon = obj.getPolygons();
                if (!polygon.empty())
                {
                    mapObj.hasPolygon = true;

                    for (const auto &point : polygon)
                    {
                        mapObj.polygonPoints.push_back({(float)pos.x + (float)point.x,
                                                        (float)pos.y + (float)point.y});
                    }
                }

                // Simpan custom properties dari object
                for (auto &[key, prop] : obj.getProperties().getProperties())
                    mapObj.properties[key] = prop;

                tilesonMap->Objects.push_back(mapObj);
            }
        }
    }

    // Step 5: Load seluruh texture tileset yang dipakai map
    auto &tilesetList = parsedMap->getTilesets();
    for (int i = 0; i < (int)tilesetList.size(); i++)
    {
        tson::Tileset *tileset = &tilesetList[i];
        std::string imagePath = "assets/textures/" + tileset->getImagePath().filename().string();
        TraceLog(LOG_INFO, "Tileson: Loading tileset: %s", imagePath.c_str());

        TilesetInfo info;

        // Cek apakah texture tile utama sudah dimuat, reuse jika ada
        if (textures[TILESET_MAP_1].id != 0 && imagePath == "assets/textures/tiles.png")
        {
            info.texture = textures[TILESET_MAP_1];
            TraceLog(LOG_INFO, "Tileson: Reusing cached texture for tiles.png");
        }
        else if (textures[TILESET_MAP_2].id != 0 && imagePath == "assets/textures/test.png")
        {
            info.texture = textures[TILESET_MAP_2];
            TraceLog(LOG_INFO, "Tileson: Reusing cached texture for test.png");
        }
        else if (textures[TILESET_PROPS].id != 0 && imagePath == "assets/textures/props.png")
        {
            info.texture = textures[TILESET_PROPS];
            TraceLog(LOG_INFO, "Tileson: Reusing cached texture for props.png");
        }
        else if (textures[TILESET_ITEMS].id != 0 && imagePath == "assets/textures/items.png")
        {
            info.texture = textures[TILESET_ITEMS];
            TraceLog(LOG_INFO, "Tileson: Reusing cached texture for items.png");
        }
        else
        {
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

        if (tilesonMap->tilesets.empty())
            tilesonMap->tilesets.push_back({});
        tilesonMap->tilesets[0].push_back(info);

        // assign semua layer ke group 0
        tilesonMap->layerTilesetGroup.assign(tilesonMap->layerCount, 0);
        TraceLog(LOG_INFO, "Tileson: Loaded tileset %s (gid %d-%d)", imagePath.c_str(), info.firstgid, info.lastgid);
    }

    TraceLog(LOG_INFO, "Tileson: Map loaded - %dx%d tiles", tilesonMap->width, tilesonMap->height);
}

/**
 * @brief Bersihkan seluruh data map yang sedang aktif
 *
 * Fungsi ini akan menghapus array tile, object map, texture tileset,
 * dan hasil parse Tileson dari map sebelumnya.
 */
void UnloadMap(void)
{
    if (tilesonMap != nullptr)
    {
        // Hapus data tile tiap layer
        for (int i = 0; i < tilesonMap->layerCount; i++)
            delete[] tilesonMap->tiles[i];
        delete[] tilesonMap->tiles;

        ClearTileProps();

        tilesonMap->Objects.clear();

        // Unload texture tileset dari GPU
        // Jangan unload jika texture sudah ada di cache (textures)
        for (auto &group : tilesonMap->tilesets)
        {
            for (auto &ts : group)
            {
                bool isCached = false;
                for (int i = 0; i < MAX_TEXTURES; i++)
                {
                    if (textures[i].id == ts.texture.id && ts.texture.id != 0)
                    {
                        isCached = true;
                        break;
                    }
                }
                if (!isCached && ts.texture.id != 0)
                    UnloadTexture(ts.texture);
            }
        }
        tilesonMap->tilesets.clear();

        delete tilesonMap;
        tilesonMap = nullptr;
    }

    DynamicObstacles.clear();

    ClearEnemies();
    itemData.ClearItems();
    parsedMap.reset();
}

/**
 * @brief Inisialisasi map pertama saat game dimulai
 *
 * Map awal yang aktif ditentukan langsung di fungsi ini.
 */
void InitMap(void)
{
    currentMapPath = "assets/maps/tutorial.json";
    LoadMap(currentMapPath.c_str());
    BuildMapObjectIndex();
}

/** @brief Generate world di map yang sudah di-load */
void RunWorldgen(uint64_t seed, bool isBossStage)
{
    WorldGenPools pools(seed);
    pools.LoadRoomPool("assets/maps/World_generation");
    pools.LoadCorridorPool("assets/maps/World_generation/corridor");

    for (auto *p : pools.GetAllRoomPrefabs())
        BuildMapObjectIndexTarget(p);

    // Index object tilesonMap harus dibangun sebelum GetSlots() — biar slot_worldgen ketemu
    BuildMapObjectIndexTarget(tilesonMap);

    WorldGenLayout layout(seed, isBossStage);
    layout.Generate();

    WorldGenCanvas canvas(tilesonMap, &pools, seed);
    auto slots = canvas.GetSlots();
    canvas.StampLayout(layout.GetGrid(), slots);

    InitWorldgenGrid(layout.GetGrid(), slots);

    BuildMapObjectIndex();

    std::vector<MapObject> exitObjects;
    for (const char *exitType : {EXIT_NORTH_TYPE_OBJECT_NAME, EXIT_SOUTH_TYPE_OBJECT_NAME,
                                 EXIT_EAST_TYPE_OBJECT_NAME, EXIT_WEST_TYPE_OBJECT_NAME})
        for (auto *obj : TilesonGetObjectsByType(exitType))
            exitObjects.push_back(*obj);

    for (auto &exitObj : exitObjects)
    {
        int tileX = (int)(exitObj.bounds.x / WG_TILE_SIZE);
        int tileY = (int)(exitObj.bounds.y / WG_TILE_SIZE);
        int col = tileX / WG_CELL_TILES;
        int row = tileY / WG_CELL_TILES;
        canvas.StampCorridor(exitObj, col, row);
    }

    pools.UnloadCorridorPool();
    pools.UnloadRoomPool();

    BuildMapObjectIndex();
}

/*==============================================================================
 * Map Rendering
 *==============================================================================*/

/**
 * @brief Render seluruh tile map yang sedang terlihat di layar
 *
 * Rendering dilakukan per layer dan hanya untuk tile dalam visible range
 * agar jumlah tile yang digambar tetap efisien.
 */
void RenderMap(void)
{
    // Skip kalau map atau tilesets belum siap
    if (tilesonMap == nullptr || tilesonMap->tilesets.empty())
        return;

    // Ambil range tile yang visible untuk frustum culling
    currentVisibleRange = GetVisibleTileRange();

    lastTilesRendered = 0;

    static bool logged = false;
    if (!logged)
    {
        TraceLog(LOG_INFO, "layerCount=%d", tilesonMap->layerCount);
        logged = true;
    }

    BeginMode2D(camera);

    // Render semua layer, tapi hanya tile yang ada di area visible
    for (int l = 0; l < tilesonMap->layerCount; l++)
    {
        for (int y = currentVisibleRange.minY; y < currentVisibleRange.maxY; y++)
        {
            for (int x = currentVisibleRange.minX; x < currentVisibleRange.maxX; x++)
            {
                lastTilesRendered++;
                int tileId = tilesonMap->tiles[l][(y * tilesonMap->width) + x];
                if (tileId == 0)
                    continue;

                // Cari tileset yang sesuai berdasarkan range gid
                int groupIdx = tilesonMap->layerTilesetGroup[l];
                auto &tilesets = tilesonMap->tilesets[groupIdx];

                auto it = std::lower_bound(tilesets.begin(), tilesets.end(), tileId,
                                           [](const TilesetInfo &t, int id)
                                           { return t.lastgid < id; });
                if (it == tilesets.end() || tileId < it->firstgid)
                    continue;
                TilesetInfo *ts = &(*it);

                static bool logged = false;
                if (!logged)
                {
                    TraceLog(LOG_INFO, "FIRST: tileId=%d", tileId);
                    logged = true;
                }

                if (ts == nullptr)
                    continue;

                // Hitung source rectangle pada texture tileset
                int adjustedId = tileId - ts->firstgid;
                int srcX = (adjustedId % ts->cols) * (FRAME_SIZE + ts->spacing);
                int srcY = (adjustedId / ts->cols) * (FRAME_SIZE + ts->spacing);

                Rectangle srcRec = {(float)srcX, (float)srcY, (float)FRAME_SIZE, (float)FRAME_SIZE};
                Rectangle dstRec = {(float)(x * FRAME_SIZE), (float)(y * FRAME_SIZE), (float)FRAME_SIZE, (float)FRAME_SIZE};

                DrawTexturePro(ts->texture, srcRec, dstRec, (Vector2){0, 0}, 0.0F, WHITE);
            }
        }
    }

    EndMode2D();
}

/**
 * @brief Hitung range tile yang sedang terlihat di layar
 *
 * Nilai range dihitung dari viewport camera dalam world space,
 * lalu dikonversi ke index tile dan di-clamp ke batas map.
 *
 * @return Range tile visible yang dipakai oleh RenderMap()
 */
TileRange GetVisibleTileRange(void)
{
    // Pojok kiri-atas dan kanan-bawah layar dalam world space
    Vector2 worldMin = GetScreenToWorld2D({0.0f, 0.0f}, camera);
    Vector2 worldMax = GetScreenToWorld2D({(float)GameScreenWidth, (float)GameScreenHeight}, camera);

    TileRange range;

    // Konversi ke tile index dan tambahkan margin biar gak ada pop-in
    range.minX = (int)floorf(worldMin.x / FRAME_SIZE) - 1;
    range.minY = (int)floorf(worldMin.y / FRAME_SIZE) - 1;
    range.maxX = (int)ceilf(worldMax.x / FRAME_SIZE) + 1;
    range.maxY = (int)ceilf(worldMax.y / FRAME_SIZE) + 1;

    // Clamp ke batas map
    if (range.minX < 0)
        range.minX = 0;
    if (range.minY < 0)
        range.minY = 0;
    if (range.maxX > tilesonMap->width)
        range.maxX = tilesonMap->width;
    if (range.maxY > tilesonMap->height)
        range.maxY = tilesonMap->height;

    return range;
}

/** @brief Dapatkan visible world rect dari camera */
Rectangle GetVisibleWorldRect(void)
{
    Vector2 worldMin = GetScreenToWorld2D({0.0f, 0.0f}, camera);
    Vector2 worldMax = GetScreenToWorld2D({(float)GameScreenWidth, (float)GameScreenHeight}, camera);
    return {worldMin.x, worldMin.y, worldMax.x - worldMin.x, worldMax.y - worldMin.y};
}

/*==============================================================================
 * Map Switching & Navigation
 *==============================================================================*/

/** @brief Pindah ke map baru di titik tujuan tertentu */
void SwitchMap(const char *newMapPath, const char *targetDoorName)
{
    // Safety check biar gak load path kosong
    if (newMapPath == nullptr || newMapPath[0] == '\0')
    {
        TraceLog(LOG_ERROR, "SwitchMap: newMapPath is null or empty");
        return;
    }

    // Simpan state map lama sebelum pindah
    if (!currentMapPath.empty())
    {
        SaveEnemiesForMap(currentMapPath);
        itemData.SaveItemsForMap(currentMapPath);
        mapHistoryStack.Push(currentMapPath, "");
    }

    // Set pending map switch - loading screen akan menangani sisanya
    gState->isSwitchingMap = true;
    gState->pendingMapPath = newMapPath;
    gState->pendingDoorName = (targetDoorName != nullptr && targetDoorName[0] != '\0')
                                  ? targetDoorName
                                  : SPAWN_OBJECT_NAME;
    // Reset loading state agar InitLoadingScreen() dipanggil ulang
    gState->enteredLoading = false;
    gState->loadingStage = 0;
    gState->loadingProgress = 0.0F;
    gState->loadingComplete = false;
    gState->loadingText = "Switching map...";
    gState->currentScreen = LOADING;

    TraceLog(LOG_INFO, "SwitchMap: transitioning to LOADING screen for map: %s", newMapPath);
}

/**
 * @brief Kembali ke map sebelumnya dari history
 *
 * Fungsi ini memuat ulang map sebelumnya lalu mengatur ulang posisi player dan camera.
 */
void GoBack(void)
{
    if (mapHistoryStack.IsEmpty())
    {
        TraceLog(LOG_WARNING, "GoBack: no map history to go back to");
        return;
    }

    // Simpan state map sekarang
    SaveEnemiesForMap(currentMapPath);
    itemData.SaveItemsForMap(currentMapPath);

    // Ambil history teratas dan pop dari stack
    MapSystem::MapHistoryEntry prev = mapHistoryStack.Pop();

    // Set pending map switch - loading screen akan menangani sisanya
    gState->isGoingBack = true;
    gState->pendingMapPath = prev.mapPath;
    gState->pendingDoorName = prev.doorName.empty() ? SPAWN_OBJECT_NAME : prev.doorName;
    // Reset loading state agar InitLoadingScreen() dipanggil ulang
    gState->enteredLoading = false;
    gState->loadingStage = 0;
    gState->loadingProgress = 0.0F;
    gState->loadingComplete = false;
    gState->loadingText = "Returning to previous map...";
    gState->currentScreen = LOADING;

    TraceLog(LOG_INFO, "GoBack: transitioning to LOADING screen for map: %s", prev.mapPath.c_str());
}

/**
 * @brief Sisakan cuma entry teratas di stack riwayat
 *
 * Pas stage transition via NextStage(), map saat ini di-push ke stack
 * sebagai prev stage. Tapi kita cuman mau nyimpen 1 prev aja — tutorial
 * atau stage yang lebih lama harus dibuang.
 */
void TrimStageStack(void)
{
    if (mapHistoryStack.IsEmpty())
    {
        TraceLog(LOG_INFO, "TrimStageStack: stack kosong");
        return;
    }

    // Ambil entry teratas (prev stage), hapus sisanya, push balik
    MapSystem::MapHistoryEntry topEntry = mapHistoryStack.Pop();
    int removedCount = 0;
    while (!mapHistoryStack.IsEmpty())
    {
        mapHistoryStack.Pop();
        removedCount++;
    }
    mapHistoryStack.Push(topEntry.mapPath, topEntry.doorName);

    TraceLog(LOG_INFO, "TrimStageStack: nyimpen [%s], buang %d entry", topEntry.mapPath.c_str(), removedCount);
}

/**
 * @brief Dapatkan path map yang sedang aktif
 * @return Path map saat ini (kosong jika tidak ada)
 */
const char *GetCurrentMapPath(void)
{
    return currentMapPath.c_str();
}

/**
 * @brief Set path map yang sedang aktif
 * @param newPath Path map baru
 */
void SetCurrentMapPath(const char *newPath)
{
    currentMapPath = (newPath != nullptr && newPath[0] != '\0') ? newPath : "";
}
