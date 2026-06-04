/**
 * @file debugmode.cpp
 * @brief Implementasi dari Debug System Module
 *
 * File ini berisi implementasi class Debug untuk:
 * - Toggle debug mode
 * - Render panel debug
 * - Render overlay debug di world space
 * - Menampilkan info runtime player, map, camera, dan collision
 */

#include "game_debug.h"
#include "../lib/raylib/include/raylib.h"
#include "input.h"
#include "../lib/raylib/include/raymath.h"
#include "screen.h"
#include "map.h"
#include "mapLogic.h"
#include "animation.h"
#include "entities.h"
#include "player.h"
#include "enemy.h"
#include <algorithm>
#include <cctype>
#include "item.h"
#include "enemy_ai.h"

/*==============================================================================
 * Global Variables
 *==============================================================================*/

/** @brief Instance global debug */
Debug DebugInstance;

/** @brief Flag debug mode */
bool isDebugMode = false;
/** @brief Tampilkan overlay flow field enemy */
bool showFlowFieldOverlay = false;
/** @brief Tampilkan overlay flow field player */
bool showFlowFieldOverlayPlayer = false;

/*==============================================================================
 * Private Helper Methods
 *==============================================================================*/

// Hitung bounds panel berdasarkan urutan tampil
Rectangle Debug::GetPanelBounds(int index, float panelWidth, float panelHeight) const
{
    // Konfigurasi grid panel debug
    const int ColumnCount = 2;
    const float StartX = 5.0f;
    const float StartY = 5.0f;
    const float GapX = 10.0f;
    const float GapY = 10.0f;

    int column = index % ColumnCount;
    int row = index / ColumnCount;

    Rectangle bounds = {
        StartX + column * (panelWidth + GapX),
        StartY + row * (panelHeight + GapY),
        panelWidth,
        panelHeight};

    return bounds;
}

// Bangun daftar panel debug yang aktif
std::vector<Debug::DebugPanelEntry> Debug::BuildActivePanels(void) const
{
    std::vector<DebugPanelEntry> panels;

    // Daftar panel aktif yang akan dirender
    // panels.push_back({"Map", &Debug::DrawMapPanel, true});
    // panels.push_back({"Camera", &Debug::DrawCameraPanel, true});
    // panels.push_back({"Player", &Debug::DrawPlayerPanel, true});
    panels.push_back({"Zoom", &Debug::DrawZoomPanel, true});
    // panels.push_back({"Frustum", &Debug::DrawFrustumPanel, true});
    // panels.push_back({"Collision", &Debug::DrawCollisionPanel, true});

    return panels;
}

// Gambar frame dasar untuk satu panel debug
void Debug::DrawPanelFrame(Rectangle bounds, const char *title, Color borderColor) const
{
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, Fade(BLACK, 0.7f));
    DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, borderColor);
    DrawText(title, (int)bounds.x + 10, (int)bounds.y + 5, 18, borderColor);
}

// Gambar overlay collision untuk layer tertentu
void Debug::DrawCollisionOverlay(const std::string &layerName, Color rectColor, Color polygonColor, Color pointColor)
{
    const std::vector<MapObject *> &objs = TilesonGetObjectsByLayerName(layerName.c_str());

    for (auto *obj : objs)
    {
        if (obj->hasPolygon)
        {
            // Gambar setiap edge dan titik polygon
            int pointCount = (int)obj->polygonPoints.size();
            if (pointCount >= 2)
            {
                for (int i = 0; i < pointCount; i++)
                {
                    int nextIndex = (i + 1) % pointCount;
                    DrawLineEx(obj->polygonPoints[i], obj->polygonPoints[nextIndex], 2.0f, polygonColor);
                    DrawCircleV(obj->polygonPoints[i], 3.0f, pointColor);
                }
            }
        }
        else
        {
            // Gambar rectangle collision
            DrawRectangleLinesEx(obj->bounds, 2.0f, rectColor);
        }
    }
}

// Gambar overlay raycast interaksi player
void Debug::DrawRaycastOverlay(void)
{
    // Gambar titik hit jika ray mengenai object (hanya saat debug mode aktif)
    if (isDebugMode && PlayerInstance.LastHit.hit)
        DrawCircleV(PlayerInstance.LastHit.point, 4.0f, RED);
}

// Gambar overlay area serangan player
void Debug::DrawAttackOverlay(void)
{
    // Hanya gambar jika player sedang menyerang
    if (!PlayerInstance.attack.active || !PlayerInstance.attack.weapon)
        return;

    Vector2 playerCenter = PlayerInstance.GetCenter();
    float reach = PlayerInstance.attack.weapon->reach;
    float breadth = PlayerInstance.attack.weapon->breadth;
    float angle = PlayerInstance.attack.raycastAngle;
    float attackerRadius = PlayerInstance.GetHitboxWidth() / 2.0f;

    float rad = angle * (PI / 180.0f);
    Vector2 forward = {cosf(rad), sinf(rad)};
    Vector2 right = {-sinf(rad), cosf(rad)};

    Vector2 edgeCenter = {
        playerCenter.x + forward.x * attackerRadius,
        playerCenter.y + forward.y * attackerRadius};

    Vector2 p1 = {edgeCenter.x + right.x * (breadth / 2.0f), edgeCenter.y + right.y * (breadth / 2.0f)};
    Vector2 p2 = {edgeCenter.x - right.x * (breadth / 2.0f), edgeCenter.y - right.y * (breadth / 2.0f)};
    Vector2 p3 = {p1.x + forward.x * reach, p1.y + forward.y * reach};
    Vector2 p4 = {p2.x + forward.x * reach, p2.y + forward.y * reach};

    // Fill rotated rectangle
    Color fillColor = Fade(RED, 0.3f);
    DrawTriangle(p1, p2, p3, fillColor);
    DrawTriangle(p2, p4, p3, fillColor);

    // Draw rotated rectangle outline
    DrawLineEx(p1, p3, 2.0f, RED);
    DrawLineEx(p3, p4, 2.0f, RED);
    DrawLineEx(p4, p2, 2.0f, RED);
    DrawLineEx(p2, p1, 2.0f, RED);

    // Draw AABB outline for visual reference (used for prop/object detection)
    float minX = std::min({p1.x, p2.x, p3.x, p4.x});
    float maxX = std::max({p1.x, p2.x, p3.x, p4.x});
    float minY = std::min({p1.y, p2.y, p3.y, p4.y});
    float maxY = std::max({p1.y, p2.y, p3.y, p4.y});
    Rectangle aabb = {minX, minY, maxX - minX, maxY - minY};
    DrawRectangleLinesEx(aabb, 1.0f, Fade(ORANGE, 0.5f));

    // Gambar titik hit jika ray mengenai object
    if (PlayerInstance.LastHit.hit)
        DrawCircleV(PlayerInstance.LastHit.point, 4.0f, VIOLET);

    // Label
    DrawText("Attack Area (Rotated OBB)", (int)minX, (int)minY - 14, 14, RED);
}

// Gambar titik spawn musuh yang terdeteksi dari data map
void Debug::DrawEnemySpawnOverlay(void)
{
    if (tilesonMap == nullptr)
        return;

    for (auto &obj : tilesonMap->Objects)
    {
        // Deteksi sederhana apakah ini objek musuh
        std::string nameLower = obj.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c)
                       { return std::tolower(c); });

        std::string typeLower = obj.type;
        std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), [](unsigned char c)
                       { return std::tolower(c); });

        bool isEnemySpawn = (nameLower.find("enemy") != std::string::npos ||
                             nameLower.find("slime") != std::string::npos ||
                             nameLower.find("skeleton") != std::string::npos ||
                             nameLower.find("wolf") != std::string::npos ||
                             obj.name == "spawn_enemy" ||
                             typeLower == "enemy_spawn");

        if (isEnemySpawn)
        {
            Vector2 pos = {obj.bounds.x, obj.bounds.y};

            // Gambar lingkaran di titik spawn
            DrawCircleV(pos, 5.0f, PURPLE);
            DrawCircleLinesV(pos, 8.0f, Fade(PURPLE, 0.5f));

            // Label nama objek
            DrawText(obj.name.c_str(), (int)pos.x - 35, (int)pos.y - 28, 12, PURPLE);

            // Tampilkan jumlah jika ada properti 'count'
            if (obj.properties.count("count"))
            {
                int count = 1;
                auto prop = obj.properties.at("count");
                if (prop.getType() == tson::Type::Int)
                    count = prop.getValue<int>();
                else if (prop.getType() == tson::Type::Float)
                    count = (int)prop.getValue<float>();
                DrawText(TextFormat("Count: %d", count), (int)pos.x + 10, (int)pos.y + 10, 10, MAGENTA);
            }

            // Tampilkan radius patroli (Lingkaran besar transparan)
            float radius = 128.0f;
            if (obj.properties.count("radius"))
            {
                auto prop = obj.properties.at("radius");
                if (prop.getType() == tson::Type::Int)
                    radius = (float)prop.getValue<int>();
                else if (prop.getType() == tson::Type::Float)
                    radius = prop.getValue<float>();
            }
            DrawCircleLinesV(pos, radius, Fade(PURPLE, 0.2f));
            DrawText(TextFormat("Rad: %.0f", radius), (int)pos.x - 20, (int)pos.y + 15, 10, MAGENTA);
        }
    }
}

/*==============================================================================
 * Public Methods
 *==============================================================================*/

/**
 * @brief Toggle debug mode saat tombol TAB ditekan
 */
void Debug::Toggle(void)
{
    const InputState& in = InputInstance.GetState();

    if (in.debugToggle)
    {
        isDebugMode = !isDebugMode;
        TraceLog(LOG_INFO, "Debug mode: %s", isDebugMode ? "ON" : "OFF");
    }
    if (in.debugToggleEnemy)
    {
        showFlowFieldOverlay = !showFlowFieldOverlay;
    }
    if (in.debugTogglePlayer)
    {
        showFlowFieldOverlayPlayer = !showFlowFieldOverlayPlayer;
    }
}

/**
 * @brief Render seluruh panel debug yang aktif
 *
 * Fungsi ini hanya berjalan saat debug mode aktif.
 */
void Debug::Draw(void)
{
    if (!isDebugMode)
        return;

    std::vector<DebugPanelEntry> panels = BuildActivePanels();

    const float DefaultPanelWidth = 320.0f;
    const float DefaultPanelHeight = 110.0f;

    int visibleIndex = 0;
    for (auto &panel : panels)
    {
        if (!panel.enabled)
            continue;

        Rectangle bounds = GetPanelBounds(visibleIndex, DefaultPanelWidth, DefaultPanelHeight);
        (this->*panel.drawFn)(bounds);
        visibleIndex++;
    }
}

/*==============================================================================
 * Debug Panels
 *==============================================================================*/

/**
 * @brief Gambar panel informasi map
 *
 * Panel ini menampilkan ukuran map, jumlah layer, dan status tileset.
 *
 * @param bounds Area panel
 */
void Debug::DrawMapPanel(Rectangle bounds)
{
    if (tilesonMap == nullptr)
        return;

    DrawPanelFrame(bounds, "[ MAP DEBUG ]", YELLOW);
    DrawText(TextFormat("Size    : %dx%d tiles", tilesonMap->width, tilesonMap->height),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText(TextFormat("Layers  : %d", tilesonMap->layerCount),
             (int)bounds.x + 10, (int)bounds.y + 47, 16, WHITE);
    DrawText(TextFormat("Tileset : %s", !tilesonMap->tilesets.empty() ? "Loaded" : "Not loaded"),
             (int)bounds.x + 10, (int)bounds.y + 67, 16, !tilesonMap->tilesets.empty() ? GREEN : RED);
}

/**
 * @brief Gambar panel informasi camera
 *
 * @param bounds Area panel
 */
void Debug::DrawCameraPanel(Rectangle bounds)
{
    DrawPanelFrame(bounds, "[ CAMERA DEBUG ]", SKYBLUE);
    DrawText(TextFormat("Target  : (%.1f, %.1f)", camera.target.x, camera.target.y),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText(TextFormat("Zoom    : %.2f", camera.zoom),
             (int)bounds.x + 10, (int)bounds.y + 47, 16, WHITE);
}

/**
 * @brief Gambar panel informasi player
 *
 * Panel ini menampilkan posisi, speed, ukuran hitbox, dan offset hitbox.
 *
 * @param bounds Area panel
 */
void Debug::DrawPlayerPanel(Rectangle bounds)
{
    DrawPanelFrame(bounds, "[ PLAYER DEBUG ]", GREEN);
    DrawText(TextFormat("Position   : (%.1f, %.1f)", PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText(TextFormat("Speed      : %.1f", PlayerInstance.Speed),
             (int)bounds.x + 10, (int)bounds.y + 47, 16, WHITE);
    DrawText(TextFormat("Hitbox Size: %.1f x %.1f", PlayerInstance.GetHitboxWidth(), PlayerInstance.GetHitboxHeight()),
             (int)bounds.x + 10, (int)bounds.y + 67, 16, YELLOW);
    DrawText(TextFormat("Hitbox Off : (%.1f, %.1f)", PlayerInstance.GetHitboxOffsetX(), PlayerInstance.GetHitboxOffsetY()),
             (int)bounds.x + 10, (int)bounds.y + 87, 16, YELLOW);
}

/**
 * @brief Gambar panel zoom debug dan handle input zoom
 *
 * Zoom hanya bisa diubah saat debug mode aktif.
 *
 * @param bounds Area panel
 */
void Debug::DrawZoomPanel(Rectangle bounds)
{
    const float MAX_ZOOM = 3.5f;
    const float MIN_ZOOM = 0.60f;
    const float ZOOM_INCREMENT = 0.25f;

    // Handle zoom dengan scroll mouse
    float mouseWheel = GetMouseWheelMove();
    if (mouseWheel != 0)
    {
        camera.zoom += mouseWheel * ZOOM_INCREMENT;
        if (camera.zoom > MAX_ZOOM)
            camera.zoom = MAX_ZOOM;
        if (camera.zoom < MIN_ZOOM)
            camera.zoom = MIN_ZOOM;
    }

    DrawPanelFrame(bounds, "[ ZOOM DEBUG ]", ORANGE);
    DrawText(TextFormat("Zoom    : %.2f", camera.zoom),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText("[Scroll] Zoom In/Out",
             (int)bounds.x + 10, (int)bounds.y + 47, 16, YELLOW);
}

/**
 * @brief Gambar panel informasi frustum culling
 *
 * Panel ini menampilkan jumlah tile yang dirender dan range tile visible.
 *
 * @param bounds Area panel
 */
void Debug::DrawFrustumPanel(Rectangle bounds)
{
    if (tilesonMap == nullptr)
        return;

    int totalMapTiles = tilesonMap->width * tilesonMap->height * tilesonMap->layerCount;

    DrawPanelFrame(bounds, "[ FRUSTUM DEBUG ]", VIOLET);
    DrawText(TextFormat("Tiles Drawn : %d", lastTilesRendered),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText(TextFormat("Total Map   : %d", totalMapTiles),
             (int)bounds.x + 10, (int)bounds.y + 47, 16, GRAY);
    DrawText(TextFormat("Range X: %d-%d", currentVisibleRange.minX, currentVisibleRange.maxX),
             (int)bounds.x + 10, (int)bounds.y + 67, 16, WHITE);
    DrawText(TextFormat("Range Y: %d-%d", currentVisibleRange.minY, currentVisibleRange.maxY),
             (int)bounds.x + 10, (int)bounds.y + 87, 16, WHITE);
}

/**
 * @brief Gambar panel informasi collision dan boundary
 *
 * Panel ini menampilkan jumlah object collision rectangle dan polygon
 * yang terdeteksi dari layer collision aktif.
 *
 * @param bounds Area panel
 */
void Debug::DrawCollisionPanel(Rectangle bounds)
{
    if (tilesonMap == nullptr)
        return;

    int collisionRectCount = 0;
    int collisionPolygonCount = 0;
    int mapBoundPolygonCount = 0;

    // Hitung object collision berdasarkan layer name
    const std::vector<MapObject *> &collisionObjs = TilesonGetObjectsByLayerName(COLLISION_LAYER_NAME);
    for (auto *obj : collisionObjs)
    {
        if (obj->hasPolygon)
            collisionPolygonCount++;
        else
            collisionRectCount++;
    }

    DrawPanelFrame(bounds, "[ COLLISION DEBUG ]", RED);
    DrawText(TextFormat("Rect Count     : %d", collisionRectCount),
             (int)bounds.x + 10, (int)bounds.y + 27, 16, WHITE);
    DrawText(TextFormat("Polygon Count  : %d", collisionPolygonCount),
             (int)bounds.x + 10, (int)bounds.y + 47, 16, WHITE);
    DrawText(TextFormat("Map Bound Poly : %d", mapBoundPolygonCount),
             (int)bounds.x + 10, (int)bounds.y + 67, 16, WHITE);
    DrawText(TextFormat("Boundary Mode  : %s", mapBoundPolygonCount > 0 ? "Custom Polygon" : "Default Rectangle"),
             (int)bounds.x + 10, (int)bounds.y + 87, 16, mapBoundPolygonCount > 0 ? GREEN : YELLOW);
}

/*==============================================================================
 * World Overlay
 *==============================================================================*/

/**
 * @brief Render overlay debug langsung di world space
 *
 * Overlay ini menampilkan hitbox player, object collision,
 * raycast interaksi, dan batas map aktif.
 */
void Debug::DrawWorldOverlay(void)
{
    if (tilesonMap == nullptr)
        return;

    // Overlay hanya muncul saat debug mode aktif
    if (!isDebugMode)
        return;

    // Gambar raycast interaksi
    DrawRaycastOverlay();

    Rectangle playerHitbox = PlayerInstance.GetHitbox();

    DrawRectangleLinesEx(playerHitbox, 2.0f, LIME);

    // Magnet radius overlay
    Vector2 playerCenter = PlayerInstance.GetCenter();
    DrawCircleLinesV(playerCenter, PlayerInstance.GetMagnetRadius(), GOLD);

    // Titik sudut hitbox player
    DrawCircleV({playerHitbox.x, playerHitbox.y}, 2.5f, LIME);
    DrawCircleV({playerHitbox.x + playerHitbox.width, playerHitbox.y}, 2.5f, LIME);
    DrawCircleV({playerHitbox.x, playerHitbox.y + playerHitbox.height}, 2.5f, LIME);
    DrawCircleV({playerHitbox.x + playerHitbox.width, playerHitbox.y + playerHitbox.height}, 2.5f, LIME);

    // Overlay collision object
    DrawCollisionOverlay(COLLISION_LAYER_NAME, RED, RED, LIGHTGRAY);
    DrawCollisionOverlay(OBJECT_LAYER_NAME, SKYBLUE, SKYBLUE, LIGHTGRAY);
    DrawCollisionOverlay(TRAP_LAYER_NAME, BEIGE, BEIGE, LIGHTGRAY);
    DrawCollisionOverlay(ITEM_LAYER_NAME, PINK, PINK, LIGHTGRAY);
    DrawCollisionOverlay(EXIT_LAYER_NAME, BLACK, BLACK, LIGHTGRAY);
    DrawAttackOverlay();
    if (showFlowFieldOverlayPlayer)
    {
        DrawFlowFieldOverlay(globalFlowField);
    }
    if (showFlowFieldOverlay)
    {
        for (auto *entity : Entities::GetRegistry())
        {
            Enemy *enemy = dynamic_cast<Enemy *>(entity);
            if (!enemy)
                continue;
            const FlowField *ff = enemy->GetReturnFlowField();
            if (ff && ff->IsReady())
                DrawFlowFieldOverlay(*ff);
        }
    }
    DrawEnemySpawnOverlay();

    // Batas luar map
    Rectangle mapBounds = {
        0.0f,
        0.0f,
        (float)tilesonMap->width * FRAME_SIZE,
        (float)tilesonMap->height * FRAME_SIZE};

    DrawRectangleLinesEx(mapBounds, 2.0f, GREEN);

    // Hitbox items
    for (auto &item : itemData.activeItems)
    {
        if (!item.isPickedUp)
        {
            const ItemDefinition &def = itemDefs.GetById(item.definitionId);
            DrawRectangleLinesEx(item.hitbox, 1.5f, PINK);
            DrawText(def.name.c_str(), (int)item.hitbox.x, (int)item.hitbox.y - 12, 10, PINK);
        }
    }
}

/**
 * @brief Gambar overlay arah flow field pada tile yang reachable.
 * @param field Flow field yang akan divisualisasikan
 */
void Debug::DrawFlowFieldOverlay(const FlowField &field)
{
    if (!field.IsReady())
        return;

    int mapW = tilesonMap->width;
    int mapH = tilesonMap->height;

    for (int y = 0; y < mapH; y++)
    {
        for (int x = 0; x < mapW; x++)
        {
            Vector2 tileCenter = {
                x * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET,
                y * FLOW_FIELD_TILE_SIZE + FLOW_FIELD_CENTER_OFFSET};

            Vector2 dir = field.GetDirection(tileCenter);
            if (dir.x == 0 && dir.y == 0)
                continue;

            Vector2 arrowEnd = {
                tileCenter.x + dir.x * (FLOW_FIELD_TILE_SIZE * 0.4f),
                tileCenter.y + dir.y * (FLOW_FIELD_TILE_SIZE * 0.4f)};

            DrawLineV(tileCenter, arrowEnd, BLUE);
            DrawCircleV(arrowEnd, 2.0f, ORANGE);
        }
    }
}

/**
 * @brief Gambar overlay debug steering untuk satu enemy.
 * @param enemy Enemy yang steering-nya akan divisualisasikan
 */
void Debug::DrawSteeringOverlay(Enemy &enemy)
{
    Vector2 pos = enemy.GetCenter();
    float tileSize = FLOW_FIELD_TILE_SIZE;

    // --- 1. Raycast line ---
    Vector2 vel = enemy.GetVelocity();
    Vector2 flowDir = globalFlowField.GetDirection(pos);
    Vector2 rayDir = (Vector2LengthSqr(vel) > 0.001f) ? Vector2Normalize(vel) : flowDir;
    Vector2 rayEnd = Vector2Add(pos, Vector2Scale(rayDir, tileSize * 2.0f));

    auto obstacles = cachedObstacleList;
    RayHitResult hit = enemy.CastDebugRay(rayDir, tileSize * 2.0f, obstacles, LINE, 0, 0);

    Color rayColor = hit.hit ? RED : GREEN;
    DrawLineV(pos, rayEnd, rayColor);
    DrawCircleV(rayEnd, 3.0f, rayColor);

    // --- 2. Steering dir arrow ---
    Vector2 steerDir = enemy.Steering.SteeringDir;
    if (Vector2LengthSqr(steerDir) > 0.001f)
    {
        Vector2 steerEnd = Vector2Add(pos, Vector2Scale(steerDir, tileSize * 0.6f));
        DrawLineV(pos, steerEnd, BLUE);
        DrawCircleV(steerEnd, 3.0f, BLUE);
    }

    // --- 3. 5x5 tile highlight --- (skip kalau in range)
    if (!enemy.Steering.IsInRangeDebug(enemy.GetCenter(), PlayerInstance.GetHitbox(), enemy.GetRayDetectionLength()))
    {
        Vector2 flowTile = enemy.Steering.LastFlowTile;
        for (int dy = -STEERING_GRID_RADIUS; dy <= STEERING_GRID_RADIUS; dy++)
        {
            for (int dx = -STEERING_GRID_RADIUS; dx <= STEERING_GRID_RADIUS; dx++)
            {
                if (dx == 0 && dy == 0)
                    continue;

                int tx = (int)flowTile.x + dx;
                int ty = (int)flowTile.y + dy;

                Vector2 tileCenter = {
                    tx * tileSize + tileSize * 0.5f,
                    ty * tileSize + tileSize * 0.5f};

                bool walkable = IsPositionSafe(tileCenter, enemy.GetHitboxValue(), enemy.GetHitboxValue(),
                                               enemy.GetOffSetValue(), enemy.GetOffSetValue());

                Rectangle tileRect = {
                    tx * tileSize, ty * tileSize,
                    tileSize, tileSize};

                DrawRectangleLinesEx(tileRect, 1.0f, walkable ? GREEN : RED);
            }
        }
    }

    // --- 4. IsPlayerInRange radius ---
    DrawCircleV(pos, enemy.GetRayDetectionLength(), Fade(YELLOW, 0.2f));
}