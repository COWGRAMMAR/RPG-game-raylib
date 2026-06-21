/**
 * @file minimap.cpp
 * @brief Implementasi minimap navigation aid.
 *
 * Scale: 1 tile map = 1 px grid minimap.
 * Render: 1x Texture2D (grid static) + 1x RenderTexture2D (fog overlay), single DrawTexturePro tiap layer.
 *
 * === ISOLATED BUILD ===
 * File ini gak nyentuh file existing lain (hanya include header existing).
 * Semua interface pake parameter eksplisit untuk grid building.
 */

#include "map/minimap.h"
#include "entities/player.h"    // PlayerInstance.GetPosition()
#include "map/map.h"            // tilesonMap, GetCurrentMapPath()
#include "map/mapLogic.h"       // gCollisionCache
#include "systems/input.h"      // InputInstance.IsToggleMap()
#include "rendering/animation.h"
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
    // DEBUG: pake warna kontras biar keliatan
    if (gid <= 0)
        return {255, 0, 0, 255};    // RED — void
    return {0, 0, 255, 255};         // BLUE — tile
}

/*==============================================================================
 * Grid Texture — Build dari tilesonMap + gCollisionCache
 *==============================================================================*/

void MinimapScreen::BuildGridTexture()
{
    if (!tilesonMap || tilesonMap->width <= 0 || tilesonMap->height <= 0 || tilesonMap->layerCount <= 0)
    {
        TraceLog(LOG_WARNING, "MINIMAP DBG: BuildGridTexture skipped — tilesonMap invalid (layers=%d)",
                 tilesonMap ? tilesonMap->layerCount : -1);
        return;
    }
    if (tilesonMap->tiles[0] == nullptr)
    {
        TraceLog(LOG_WARNING, "MINIMAP DBG: BuildGridTexture skipped — tiles[0] is null");
        return;
    }

    int mapW = tilesonMap->width;
    int mapH = tilesonMap->height;
    int gw = g_Minimap.gridWidth;
    int gh = g_Minimap.gridHeight;
    if (gw <= 0 || gh <= 0)
    {
        TraceLog(LOG_WARNING, "MINIMAP DBG: BuildGridTexture skipped — gw/gh invalid (%dx%d)", gw, gh);
        return;
    }

    // Hitung sample GIDs untuk debugging
    int nonZeroGids = 0;
    int totalCells  = gw * gh;

    Image img = GenImageColor(gw, gh, BLANK);
    if (img.data == nullptr)
    {
        TraceLog(LOG_WARNING, "MINIMAP DBG: GenImageColor failed (%dx%d)", gw, gh);
        return;
    }

    Color* pixels = (Color*)img.data;

    for (int ty = 0; ty < gh; ty++)
    {
        for (int tx = 0; tx < gw; tx++)
        {
            int idx  = ty * gw + tx;
            int gid  = tilesonMap->tiles[0][ty * mapW + tx];
            pixels[idx] = MapGidToColor(gid);
            if (gid > 0) nonZeroGids++;

            Rectangle tileRect = {(float)(tx * FRAME_SIZE), (float)(ty * FRAME_SIZE),
                                  (float)FRAME_SIZE, (float)FRAME_SIZE};
            for (const auto& obs : gCollisionCache.rects)
            {
                if (CheckCollisionRecs(tileRect, obs))
                {
                    pixels[idx] = {255, 0, 0, 255};
                    break;
                }
            }
        }
    }

    gridTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    TraceLog(LOG_INFO, "MINIMAP DBG: BuildGridTexture gw=%d gh=%d mapW=%d mapH=%d layers=%d | nonZeroGids=%d/%d | gridTexID=%d",
             gw, gh, mapW, mapH, tilesonMap->layerCount, nonZeroGids, totalCells, gridTexture.id);

    if (gridTexture.id > 0)
    {
        SetTextureFilter(gridTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(gridTexture, TEXTURE_WRAP_CLAMP);
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
    gridTexture = {0};
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

    fogRT  = LoadRenderTexture(w, h);
    if (fogRT.id <= 0)
    {
        TraceLog(LOG_WARNING, "MINIMAP DBG: Init fogRT failed (%dx%d)", w, h);
        return;
    }

    TraceLog(LOG_INFO, "MINIMAP DBG: Init w=%d h=%d fogRTID=%d", w, h, fogRT.id);
    SetTextureFilter(fogRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(fogRT.texture, TEXTURE_WRAP_CLAMP);

    CalculateLayout();
    BuildGridTexture();

    initialized = true;
}

void MinimapScreen::Shutdown()
{
    if (gridTexture.id > 0)
        UnloadTexture(gridTexture);
    if (fogRT.id > 0)
        UnloadRenderTexture(fogRT);

    gridTexture = {0};
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
    // Panel dan viewport pake ukuran FIXED — gak tergantung ukuran map
    int panelW = MINIMAP_VIEWPORT_WIDTH  + MINIMAP_PANEL_PADDING * 2;
    int panelH = MINIMAP_VIEWPORT_HEIGHT + MINIMAP_PANEL_PADDING * 2;

    float px = (GameScreenWidth  - panelW) / 2.0f;
    float py = (GameScreenHeight - panelH) / 2.0f;

    panelRect = {px, py, (float)panelW, (float)panelH};
    viewRect  = {px + MINIMAP_PANEL_PADDING, py + MINIMAP_PANEL_PADDING,
                 (float)MINIMAP_VIEWPORT_WIDTH, (float)MINIMAP_VIEWPORT_HEIGHT};
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
    int tileSize = FRAME_SIZE;

    int gridX = ((int)playerPos.x / tileSize) / MINIMAP_TILE_TO_PX;
    int gridY = ((int)playerPos.y / tileSize) / MINIMAP_TILE_TO_PX;

    float sx = viewRect.x + panOffset.x
               + (float)gridX * MINIMAP_PANEL_SCALE + MINIMAP_PANEL_SCALE / 2.0f;
    float sy = viewRect.y + panOffset.y
               + (float)gridY * MINIMAP_PANEL_SCALE + MINIMAP_PANEL_SCALE / 2.0f;

    // Marker radius scaling proporsional sama PANEL_SCALE
    float innerR = fmaxf(MINIMAP_PANEL_SCALE * 1.0f, 4.0f);
    float outerR = innerR * 1.5f;
    DrawCircle((int)sx, (int)sy, innerR, {100, 255, 100, 255});
    DrawCircle((int)sx, (int)sy, outerR, {100, 255, 100, 80});  // outer glow
}

/*==============================================================================
 * MinimapScreen - View (Auto-Follow Player)
 *==============================================================================*/

void MinimapScreen::UpdateView()
{
    if (!active || !initialized)
        return;

    float scaledW = (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE;
    float scaledH = (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE;

    Vector2 playerPos = PlayerInstance.GetPosition();
    int playerGridX = ((int)playerPos.x / FRAME_SIZE) / MINIMAP_TILE_TO_PX;
    int playerGridY = ((int)playerPos.y / FRAME_SIZE) / MINIMAP_TILE_TO_PX;

    if (scaledW > viewRect.width)
    {
        panOffset.x = (viewRect.width / 2.0f)
            - ((float)playerGridX * MINIMAP_PANEL_SCALE) - (MINIMAP_PANEL_SCALE / 2.0f);
        panOffset.x = std::clamp(panOffset.x, viewRect.width - scaledW, 0.0f);
    }
    else
    {
        panOffset.x = (viewRect.width - scaledW) / 2.0f;
    }

    if (scaledH > viewRect.height)
    {
        panOffset.y = (viewRect.height / 2.0f)
            - ((float)playerGridY * MINIMAP_PANEL_SCALE) - (MINIMAP_PANEL_SCALE / 2.0f);
        panOffset.y = std::clamp(panOffset.y, viewRect.height - scaledH, 0.0f);
    }
    else
    {
        panOffset.y = (viewRect.height - scaledH) / 2.0f;
    }
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

    // Kalo user lagi drag, jangan auto-follow — biar manual pan dulu
    if (!isDragging)
        UpdateView();

    HandlePan(mousePosition, mouseClicked);
    UpdateMinimapFog(PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y, FRAME_SIZE);
}

void MinimapScreen::Draw(Vector2 mousePosition)
{
    if (!active || !initialized)
        return;

    (void)mousePosition;


    // Background panel
    DrawRectangleRec(panelRect, {25, 25, 30, 220});
    DrawRectangleLinesEx(panelRect, 1, {80, 78, 75, 255});

    // FOG DISABLED — debugging
    // RenderFogLayer();

    BeginScissorMode((int)viewRect.x, (int)viewRect.y,
                     (int)viewRect.width, (int)viewRect.height);

    // Grid texture (regular Texture2D — positive height, bukan render texture)
    Rectangle srcGrid = {0, 0,
                         (float)g_Minimap.gridWidth,
                         (float)g_Minimap.gridHeight};
    Rectangle dst = {viewRect.x + panOffset.x,
                     viewRect.y + panOffset.y,
                     (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE,
                     (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE};

    DrawTexturePro(gridTexture, srcGrid, dst, {0, 0}, 0.0f, WHITE);
    // DrawFogLayer();
    DrawPlayerMarker();

    EndScissorMode();
}

/*==============================================================================
 * MinimapSystem — Public Namespace Implementation
 *==============================================================================*/

void MinimapSystem::InitWithMap()
{
    // Simpan fog current map ke cache sebelum ganti
    const char* oldPath = GetCurrentMapPath();
    if (oldPath && oldPath[0] != '\0')
        SaveMinimapFogToCache(oldPath);

    // Shutdown jika sebelumnya sudah initialized
    g_MinimapScreen.Shutdown();

    // Cek tilesonMap valid
    if (!tilesonMap || tilesonMap->width <= 0 || tilesonMap->height <= 0 || tilesonMap->layerCount <= 0)
    {
        TraceLog(LOG_WARNING, "MINIMAP: InitWithMap skipped — tilesonMap invalid");
        return;
    }

    int mapW = tilesonMap->width;
    int mapH = tilesonMap->height;

    // Set dimensi grid → sampling langsung dari tilesonMap tiap Init/rebuild
    g_Minimap.mapTileWidth  = mapW;
    g_Minimap.mapTileHeight = mapH;
    g_Minimap.gridWidth  = std::max(1, mapW / MINIMAP_TILE_TO_PX);
    g_Minimap.gridHeight = std::max(1, mapH / MINIMAP_TILE_TO_PX);
    g_Minimap.fog.resize(g_Minimap.gridWidth * g_Minimap.gridHeight,
                         (unsigned char)FogState::UNEXPLORED);

    // Restore fog untuk map baru
    const char* newPath = GetCurrentMapPath();
    if (newPath && newPath[0] != '\0')
        RestoreMinimapFogFromCache(newPath);
    else
        ResetMinimapFog();

    g_MinimapScreen.Init();
}

void MinimapSystem::Shutdown()
{
    const char* path = GetCurrentMapPath();
    if (path && path[0] != '\0')
        SaveMinimapFogToCache(path);
    g_MinimapScreen.Shutdown();
}

void MinimapSystem::Update()
{
    // Update fog tiap frame berdasarkan posisi player
    UpdateMinimapFog(PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y, FRAME_SIZE);

    // Toggle minimap via M key — skip kalo inventory lagi buka
    if (InputInstance.IsToggleMap() && !InputInstance.IsInventoryOpen())
    {
        if (g_MinimapScreen.IsActive())
            g_MinimapScreen.Hide();
        else
            g_MinimapScreen.Show();
    }

    // Safety sync: kalo inventory kebuka lewat jalur lain, minimap ikut tutup
    if (InputInstance.IsInventoryOpen() && g_MinimapScreen.IsActive())
        g_MinimapScreen.Hide();

    // Update view & fog kalo minimap aktif
    if (g_MinimapScreen.IsActive())
    {
        Vector2 mousePos = GetVirtualMousePosition(gState);
        bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        g_MinimapScreen.Update(gState, mousePos, clicked);
    }
}

void MinimapSystem::Draw()
{
    if (!g_MinimapScreen.IsActive())
        return;
    Vector2 mousePos = GetVirtualMousePosition(gState);
    g_MinimapScreen.Draw(mousePos);
}
