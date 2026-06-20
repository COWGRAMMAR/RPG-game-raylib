/**
 * @file minimap.cpp
 * @brief Implementasi minimap navigation aid.
 *
 * Scale: 2 tile map -> 1 px grid minimap.
 * Render: 2x RenderTexture2D (grid static + fog dinamis), single DrawTexturePro tiap layer.
 *
 * === ISOLATED BUILD ===
 * File ini gak nyentuh file existing lain (hanya include header existing).
 * Semua interface pake parameter eksplisit untuk grid building.
 */

#include "map/minimap.h"
#include "entities/player.h"    // PlayerInstance.GetPosition()
#include <cmath>
#include <algorithm>

/*==============================================================================
 * Globals
 *==============================================================================*/

MinimapData g_Minimap;
MinimapScreen g_MinimapScreen;

/*==============================================================================
 * Color Mapping
 *==============================================================================*/

Color MapGidToColor(int gid)
{
    // GID 0 = no tile (void)
    if (gid <= 0)
        return {20, 20, 20, 255};

    /*
     * Heuristic sederhana berdasarkan GID range.
     * Bakal diperbaiki pas tileset properti udah jelas.
     *
     * Range umum (default tileset):
     *   1-15   -> floor / lantai
     *   16-47  -> wall / tembok
     *   48+    -> detail / dekorasi
     */
    if (gid >= 16 && gid <= 47)
        return {70, 65, 55, 255};      // WALL - dark brown/gray

    if (gid >= 1 && gid <= 15)
        return {160, 140, 100, 255};   // FLOOR - light brown/tan

    // Detail / lainnya - medium gray biar kontras sama wall
    return {120, 115, 105, 255};
}

/*==============================================================================
 * Grid Building
 *==============================================================================*/

static bool BlockIntersectsObstacle(int bx, int by, const std::vector<Rectangle>& obstacleRects)
{
    Rectangle blockRect = {
        (float)(bx * MINIMAP_TILE_TO_PX),
        (float)(by * MINIMAP_TILE_TO_PX),
        (float)MINIMAP_TILE_TO_PX,
        (float)MINIMAP_TILE_TO_PX
    };

    for (const auto& obs : obstacleRects)
    {
        if (CheckCollisionRecs(blockRect, obs))
            return true;
    }
    return false;
}

void BuildMinimapGrid(int mapTileW, int mapTileH,
                      const std::vector<int>& tileGids,
                      const std::vector<Rectangle>& obstacleRects)
{
    int halfW = std::max(1, mapTileW / MINIMAP_TILE_TO_PX);
    int halfH = std::max(1, mapTileH / MINIMAP_TILE_TO_PX);

    g_Minimap.mapTileWidth  = mapTileW;
    g_Minimap.mapTileHeight = mapTileH;
    g_Minimap.gridWidth  = halfW;
    g_Minimap.gridHeight = halfH;
    g_Minimap.grid.resize(halfW * halfH);
    g_Minimap.fog.resize(halfW * halfH, (unsigned char)FogState::UNEXPLORED);

    // Iterasi per 2x2 block
    for (int by = 0; by < halfH; by++)
    {
        for (int bx = 0; bx < halfW; bx++)
        {
            int idx = by * halfW + bx;

            // Step 1: cek obstacle override dulu
            if (BlockIntersectsObstacle(bx, by, obstacleRects))
            {
                g_Minimap.grid[idx] = {70, 65, 55, 255};  // WALL
                continue;
            }

            // Step 2: sampling 4 tile dalam block, dominan GID
            int gidCounts[256] = {0};
            int maxCount = 0;
            int dominantGid = 0;

            for (int dy = 0; dy < MINIMAP_TILE_TO_PX; dy++)
            {
                for (int dx = 0; dx < MINIMAP_TILE_TO_PX; dx++)
                {
                    int tx = bx * MINIMAP_TILE_TO_PX + dx;
                    int ty = by * MINIMAP_TILE_TO_PX + dy;

                    if (tx < mapTileW && ty < mapTileH)
                    {
                        int gid = tileGids[ty * mapTileW + tx];
                        if (gid >= 0 && gid < 256)
                        {
                            gidCounts[gid]++;
                            if (gidCounts[gid] > maxCount)
                            {
                                maxCount = gidCounts[gid];
                                dominantGid = gid;
                            }
                        }
                    }
                }
            }

            if (dominantGid <= 0)
            {
                g_Minimap.grid[idx] = {20, 20, 20, 255};  // void
                continue;
            }

            if (dominantGid >= 1 && dominantGid <= 15)
            {
                g_Minimap.grid[idx] = {160, 140, 100, 255};  // floor
                continue;
            }

            if (dominantGid >= 16 && dominantGid <= 47)
            {
                g_Minimap.grid[idx] = {70, 65, 55, 255};  // wall
                continue;
            }

            g_Minimap.grid[idx] = MapGidToColor(dominantGid);
        }
    }
}

/*==============================================================================
 * Fog Functions
 *==============================================================================*/

void ResetMinimapFog()
{
    std::fill(g_Minimap.fog.begin(), g_Minimap.fog.end(),
              (unsigned char)FogState::UNEXPLORED);
}

void UpdateMinimapFog(float worldX, float worldY, int tileSize)
{
    int playerTileX = ((int)worldX) / tileSize;
    int playerTileY = ((int)worldY) / tileSize;
    int gridX = playerTileX / MINIMAP_TILE_TO_PX;
    int gridY = playerTileY / MINIMAP_TILE_TO_PX;
    int radius = MINIMAP_REVEAL_RADIUS / MINIMAP_TILE_TO_PX;

    int w = g_Minimap.gridWidth;
    int h = g_Minimap.gridHeight;
    if (w <= 0 || h <= 0) return;

    // Step 1: visible sebelumnya -> explored (fogged)
    for (int i = 0; i < w * h; i++)
    {
        if (g_Minimap.fog[i] == (unsigned char)FogState::VISIBLE)
            g_Minimap.fog[i] = (unsigned char)FogState::EXPLORED;
    }

    // Step 2: reveal sekitar player (circular radius)
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            if (dx * dx + dy * dy > radius * radius)
                continue;

            int gx = gridX + dx;
            int gy = gridY + dy;

            if (gx < 0 || gx >= w || gy < 0 || gy >= h)
                continue;

            g_Minimap.fog[gy * w + gx] = (unsigned char)FogState::VISIBLE;
        }
    }
}

void SaveMinimapFogToCache(const std::string& mapPath)
{
    if (g_Minimap.gridWidth <= 0 || g_Minimap.gridHeight <= 0)
        return;

    g_Minimap.fogCache[mapPath] = g_Minimap.fog;
}

void RestoreMinimapFogFromCache(const std::string& mapPath)
{
    auto it = g_Minimap.fogCache.find(mapPath);
    if (it != g_Minimap.fogCache.end())
    {
        g_Minimap.fog = it->second;

        int expectedSize = g_Minimap.gridWidth * g_Minimap.gridHeight;
        if ((int)g_Minimap.fog.size() != expectedSize)
            g_Minimap.fog.resize(expectedSize, (unsigned char)FogState::UNEXPLORED);

        for (auto& f : g_Minimap.fog)
        {
            if (f == (unsigned char)FogState::VISIBLE)
                f = (unsigned char)FogState::EXPLORED;
        }
    }
    else
    {
        ResetMinimapFog();
    }
}

/*==============================================================================
 * MinimapScreen - Constructor / Destructor
 *==============================================================================*/

MinimapScreen::MinimapScreen()
    : active(false)
    , initialized(false)
    , isDragging(false)
    , panOffset{0, 0}
    , dragStart{0, 0}
{
    gridRT = {0};
    fogRT  = {0};
}

MinimapScreen::~MinimapScreen()
{
    Shutdown();
}

/*==============================================================================
 * MinimapScreen - Init / Shutdown
 *==============================================================================*/

void MinimapScreen::Init()
{
    if (initialized)
        return;

    int w = g_Minimap.gridWidth;
    int h = g_Minimap.gridHeight;
    if (w <= 0 || h <= 0)
        return;

    gridRT = LoadRenderTexture(w, h);
    fogRT  = LoadRenderTexture(w, h);

    if (gridRT.id <= 0 || fogRT.id <= 0)
        return;

    SetTextureFilter(gridRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fogRT.texture,  TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(gridRT.texture, TEXTURE_WRAP_CLAMP);
    SetTextureWrap(fogRT.texture,  TEXTURE_WRAP_CLAMP);

    CalculateLayout();
    PreRenderGrid();

    initialized = true;
}

void MinimapScreen::Shutdown()
{
    if (gridRT.id > 0)
        UnloadRenderTexture(gridRT);
    if (fogRT.id > 0)
        UnloadRenderTexture(fogRT);

    gridRT = {0};
    fogRT  = {0};
    initialized = false;
    active = false;
}

/*==============================================================================
 * MinimapScreen - Show / Hide / IsActive
 *==============================================================================*/

void MinimapScreen::Show()
{
    if (!initialized)
        Init();
    if (!initialized)
        return;

    active = true;

    float scaledW = (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE;
    float scaledH = (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE;

    panOffset.x = (viewRect.width  - scaledW) / 2.0f;
    panOffset.y = (viewRect.height - scaledH) / 2.0f;

    if (scaledW > viewRect.width)
        panOffset.x = viewRect.width - scaledW;
    if (scaledH > viewRect.height)
        panOffset.y = viewRect.height - scaledH;
}

void MinimapScreen::Hide()
{
    active = false;
}

bool MinimapScreen::IsActive() const
{
    return active;
}

/*==============================================================================
 * MinimapScreen - Layout
 *==============================================================================*/

void MinimapScreen::CalculateLayout()
{
    int gw = g_Minimap.gridWidth;
    int gh = g_Minimap.gridHeight;
    if (gw <= 0 || gh <= 0) return;

    int contentW = gw * MINIMAP_PANEL_SCALE;
    int contentH = gh * MINIMAP_PANEL_SCALE;

    int panelW = contentW + MINIMAP_PANEL_PADDING * 2;
    int panelH = contentH + MINIMAP_PANEL_PADDING * 2;

    float px = (GameScreenWidth  - panelW) / 2.0f;
    float py = (GameScreenHeight - panelH) / 2.0f;

    panelRect = {px, py, (float)panelW, (float)panelH};
    viewRect  = {px + MINIMAP_PANEL_PADDING, py + MINIMAP_PANEL_PADDING,
                 (float)contentW, (float)contentH};
}

/*==============================================================================
 * MinimapScreen - PreRenderGrid (static grid ke gridRT)
 *==============================================================================*/

void MinimapScreen::PreRenderGrid()
{
    int w = g_Minimap.gridWidth;
    int h = g_Minimap.gridHeight;
    if (w <= 0 || h <= 0 || gridRT.id <= 0)
        return;

    BeginTextureMode(gridRT);
    ClearBackground(BLANK);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            DrawRectangle(x, y, 1, 1, g_Minimap.grid[y * w + x]);

    EndTextureMode();
}

/*==============================================================================
 * MinimapScreen - Fog Layer
 *==============================================================================*/

void MinimapScreen::RenderFogLayer()
{
    int w = g_Minimap.gridWidth;
    int h = g_Minimap.gridHeight;
    if (w <= 0 || h <= 0 || fogRT.id <= 0)
        return;

    BeginTextureMode(fogRT);
    ClearBackground(BLANK);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            FogState state = (FogState)g_Minimap.fog[y * w + x];

            if (state == FogState::UNEXPLORED)
                DrawRectangle(x, y, 1, 1, {20, 20, 20, 255});
            else if (state == FogState::EXPLORED)
                DrawRectangle(x, y, 1, 1, Fade(BLACK, MINIMAP_FOG_EXPLORED_ALPHA));
            // VISIBLE: BLANK (transparan)
        }
    }

    EndTextureMode();
}

void MinimapScreen::DrawFogLayer() const
{
    if (fogRT.id <= 0) return;

    Rectangle src = {0, 0,
                     (float)g_Minimap.gridWidth,
                     -(float)g_Minimap.gridHeight};  // flip Y
    Rectangle dst = {viewRect.x + panOffset.x,
                     viewRect.y + panOffset.y,
                     (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE,
                     (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE};

    DrawTexturePro(fogRT.texture, src, dst, {0, 0}, 0.0f, WHITE);
}

/*==============================================================================
 * MinimapScreen - Player Marker
 *==============================================================================*/

void MinimapScreen::DrawPlayerMarker() const
{
    Vector2 playerPos = PlayerInstance.GetPosition();
    int tileSize = 16;  // TILE_SIZE

    int gridX = ((int)playerPos.x / tileSize) / MINIMAP_TILE_TO_PX;
    int gridY = ((int)playerPos.y / tileSize) / MINIMAP_TILE_TO_PX;

    float sx = viewRect.x + panOffset.x
               + (float)gridX * MINIMAP_PANEL_SCALE + MINIMAP_PANEL_SCALE / 2.0f;
    float sy = viewRect.y + panOffset.y
               + (float)gridY * MINIMAP_PANEL_SCALE + MINIMAP_PANEL_SCALE / 2.0f;

    DrawCircle((int)sx, (int)sy, 3.0f, {100, 255, 100, 255});
    DrawCircle((int)sx, (int)sy, 5.0f, {100, 255, 100, 80});  // outer glow
}

/*==============================================================================
 * MinimapScreen - Pan Handling
 *==============================================================================*/

void MinimapScreen::HandlePan(Vector2 mousePosition, bool mouseClicked)
{
    if (mouseClicked && CheckCollisionPointRec(mousePosition, viewRect))
    {
        isDragging = true;
        dragStart = mousePosition;
        return;
    }

    if (isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        Vector2 delta = {
            mousePosition.x - dragStart.x,
            mousePosition.y - dragStart.y
        };

        panOffset.x += delta.x;
        panOffset.y += delta.y;
        dragStart = mousePosition;

        float scaledW = (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE;
        float scaledH = (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE;

        if (scaledW > viewRect.width)
        {
            panOffset.x = std::clamp(panOffset.x,
                                     viewRect.width - scaledW, 0.0f);
        }
        else
        {
            panOffset.x = (viewRect.width - scaledW) / 2.0f;
        }

        if (scaledH > viewRect.height)
        {
            panOffset.y = std::clamp(panOffset.y,
                                     viewRect.height - scaledH, 0.0f);
        }
        else
        {
            panOffset.y = (viewRect.height - scaledH) / 2.0f;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        isDragging = false;

    if (isDragging && !CheckCollisionPointRec(mousePosition, viewRect))
        isDragging = false;
}

/*==============================================================================
 * MinimapScreen - Update / Draw
 *==============================================================================*/

void MinimapScreen::Update(GameState* state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active || !initialized)
        return;

    (void)state;

    HandlePan(mousePosition, mouseClicked);

    Vector2 playerPos = PlayerInstance.GetPosition();
    UpdateMinimapFog(playerPos.x, playerPos.y, 16);
}

void MinimapScreen::Draw(Vector2 mousePosition)
{
    if (!active || !initialized)
        return;

    (void)mousePosition;

    // Background panel
    DrawRectangleRec(panelRect, {25, 25, 30, 220});
    DrawRectangleLinesEx(panelRect, 1, {80, 78, 75, 255});

    BeginScissorMode((int)viewRect.x, (int)viewRect.y,
                     (int)viewRect.width, (int)viewRect.height);

    RenderFogLayer();

    Rectangle srcGrid = {0, 0,
                         (float)g_Minimap.gridWidth,
                         -(float)g_Minimap.gridHeight};
    Rectangle dst = {viewRect.x + panOffset.x,
                     viewRect.y + panOffset.y,
                     (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE,
                     (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE};

    DrawTexturePro(gridRT.texture, srcGrid, dst, {0, 0}, 0.0f, WHITE);
    DrawFogLayer();
    DrawPlayerMarker();

    EndScissorMode();
}
