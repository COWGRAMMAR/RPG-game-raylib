/**
 * @file minimap.cpp
 * @brief Implementasi minimap navigation aid — high-res grid + fog of war.
 *
 * Grid texture dibuild di MINIMAP_TILE_PX px per tile dari tileset asli,
 * fog dirender di render texture resolusi sama, single DrawTexturePro tiap layer.
 *
 * === ISOLATED BUILD ===
 * File ini gak nyentuh file existing lain (hanya include header existing).
 * Semua interface pake parameter eksplisit untuk grid building.
 */

#include "map/minimap.h"
#include "entities/player.h"    // PlayerInstance.GetPosition()
#include "map/map.h"            // tilesonMap, GetCurrentMapPath()

#include "systems/input.h"      // InputInstance.IsToggleMap()
#include "systems/keybindManager.h"  // keybindManager
#include "rendering/fonts.h"    // FontId
#include "rendering/animation.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

/*==============================================================================
 * Globals
 *==============================================================================*/

MinimapData g_Minimap;
MinimapScreen g_MinimapScreen;

/*==============================================================================
 * Grid Texture — Build dari tilesonMap (multi-layer composite, high-res)
 *==============================================================================*/

void MinimapScreen::BuildGridTexture()
{
    if (!tilesonMap || tilesonMap->width <= 0 || tilesonMap->height <= 0 || tilesonMap->layerCount <= 0)
        return;
    if (tilesonMap->tiles[0] == nullptr)
        return;

    int mapW = tilesonMap->width;
    int mapH = tilesonMap->height;
    int gw = g_Minimap.gridWidth;
    int gh = g_Minimap.gridHeight;
    if (gw <= 0 || gh <= 0)
        return;

    // High-res grid: tiap tile diwakili MINIMAP_TILE_PX x MINIMAP_TILE_PX pixel
    int texW = gw * MINIMAP_TILE_PX;
    int texH = gh * MINIMAP_TILE_PX;
    Image img = GenImageColor(texW, texH, BLANK);
    if (img.data == nullptr)
        return;

    Color* dstPixels = (Color*)img.data;

    // ── Phase 1: Pre-sample unique GIDs ──
    // Kumpulin GID unik dari SEMUA layer, lalu sample TILE_PX×TILE_PX block-nya
    std::unordered_map<int, std::vector<Color>> tileSampleCache;
    std::unordered_map<std::string, Image> loadedImages;
    {
        std::unordered_set<int> uniqueGids;
        for (int l = 0; l < tilesonMap->layerCount; l++)
        {
            for (int i = 0; i < mapW * mapH; i++)
            {
                int gid = tilesonMap->tiles[l][i];
                if (gid > 0)
                    uniqueGids.insert(gid);
            }
        }

        for (int gid : uniqueGids)
        {
            const TilesetInfo* ts = nullptr;
            for (const auto& group : tilesonMap->tilesets)
            {
                for (const auto& t : group)
                {
                    if (gid >= t.firstgid && gid <= t.lastgid)
                    { ts = &t; break; }
                }
                if (ts) break;
            }
            if (!ts) continue;

            // Load tileset image (cached per file path)
            Image tsImg = {0};
            auto it = loadedImages.find(ts->imagePath);
            if (it != loadedImages.end())
                tsImg = it->second;
            else
            {
                tsImg = LoadImage(ts->imagePath.c_str());
                if (tsImg.data == nullptr) continue;
                loadedImages[ts->imagePath] = tsImg;
            }

            // Posisi tile di spritesheet
            int adjustedId = gid - ts->firstgid;
            int imgX = (adjustedId % ts->cols) * (FRAME_SIZE + ts->spacing);
            int imgY = (adjustedId / ts->cols) * (FRAME_SIZE + ts->spacing);
            if (imgX + FRAME_SIZE > tsImg.width || imgY + FRAME_SIZE > tsImg.height)
                continue;

            // Sample TILE_PX×TILE_PX block (nearest-neighbor subsample)
            std::vector<Color> block(MINIMAP_TILE_PX * MINIMAP_TILE_PX);
            Color* srcPx = (Color*)tsImg.data;
            for (int py = 0; py < MINIMAP_TILE_PX; py++)
            {
                for (int px = 0; px < MINIMAP_TILE_PX; px++)
                {
                    int sx = imgX + (px * FRAME_SIZE) / MINIMAP_TILE_PX;
                    int sy = imgY + (py * FRAME_SIZE) / MINIMAP_TILE_PX;
                    block[py * MINIMAP_TILE_PX + px] = srcPx[sy * tsImg.width + sx];
                }
            }
            tileSampleCache[gid] = std::move(block);
        }

        for (auto& [path, tsImg] : loadedImages)
        {
            if (tsImg.data != nullptr)
                UnloadImage(tsImg);
        }
        loadedImages.clear();
    }

    // ── Phase 2: Composite per-pixel, bottom-up ──
    // Tiap pixel: layer bawah diisi dulu, layer atas overwrite kalo alpha-nya solid
    const int ALPHA_THRESHOLD = 128;

    for (int ty = 0; ty < gh; ty++)
    {
        for (int tx = 0; tx < gw; tx++)
        {
            int cellGids[32];
            int activeLayers = 0;
            for (int l = 0; l < tilesonMap->layerCount; l++)
            {
                int gid = tilesonMap->tiles[l][ty * mapW + tx];
                cellGids[l] = gid;
                if (gid > 0) activeLayers++;
            }

            // Kalo cuma 1 layer aktif — langsung blit (fast path)
            if (activeLayers == 1)
            {
                int topGid = 0;
                for (int l = tilesonMap->layerCount - 1; l >= 0; l--)
                {
                    if (cellGids[l] > 0) { topGid = cellGids[l]; break; }
                }

                auto it = tileSampleCache.find(topGid);
                if (it == tileSampleCache.end()) continue;

                const auto& block = it->second;
                for (int py = 0; py < MINIMAP_TILE_PX; py++)
                {
                    for (int px = 0; px < MINIMAP_TILE_PX; px++)
                    {
                        int dstIdx = (ty * MINIMAP_TILE_PX + py) * texW
                                     + (tx * MINIMAP_TILE_PX + px);
                        dstPixels[dstIdx] = block[py * MINIMAP_TILE_PX + px];
                    }
                }
                continue;
            }

            // Kalo multi-layer — composite per-pixel bottom-up
            for (int py = 0; py < MINIMAP_TILE_PX; py++)
            {
                for (int px = 0; px < MINIMAP_TILE_PX; px++)
                {
                    int dstIdx = (ty * MINIMAP_TILE_PX + py) * texW
                                 + (tx * MINIMAP_TILE_PX + px);
                    Color finalColor = {0, 0, 0, 0};

                    // Bottom → top: layer atas overwrite kalo alpha >= threshold
                    for (int l = 0; l < tilesonMap->layerCount; l++)
                    {
                        int gid = cellGids[l];
                        if (gid <= 0) continue;

                        auto it = tileSampleCache.find(gid);
                        if (it == tileSampleCache.end()) continue;

                        Color c = it->second[py * MINIMAP_TILE_PX + px];
                        if (c.a >= ALPHA_THRESHOLD)
                            finalColor = c;
                    }

                    dstPixels[dstIdx] = finalColor;
                }
            }
        }
    }

    gridTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    if (gridTexture.id > 0)
    {
        SetTextureFilter(gridTexture, TEXTURE_FILTER_POINT);
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
    fogRT = {0};
    bgArtwork  = {0};
    followPlayer = true;
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

    // Fog render texture: full resolution (1 tile = MINIMAP_TILE_PX pixel)
    fogRT = LoadRenderTexture(w * MINIMAP_TILE_PX, h * MINIMAP_TILE_PX);
    if (fogRT.id <= 0)
    {
        TraceLog(LOG_WARNING, "MINIMAP: Init fogRT failed (%dx%d)", w * MINIMAP_TILE_PX, h * MINIMAP_TILE_PX);
        return;
    }
    TraceLog(LOG_INFO, "MINIMAP: Init fogRT w=%d h=%d id=%d", w * MINIMAP_TILE_PX, h * MINIMAP_TILE_PX, fogRT.id);

    // Load background artwork
    if (bgArtwork.id <= 0)
    {
        bgArtwork = LoadTexture("assets/textures/minimap/mapBG.png");
        if (bgArtwork.id > 0)
            TraceLog(LOG_INFO, "MINIMAP: Loaded mapBG.png (%dx%d)", bgArtwork.width, bgArtwork.height);
        else
            TraceLog(LOG_WARNING, "MINIMAP: mapBG.png not found");
    }

    // Load fog render shader (sekali aja, reuse antar map)
    if (fogRenderShader.id == 0)
    {
        fogRenderShader = LoadShader(0, "assets/shaders/fog_render.fs");
        if (fogRenderShader.id > 0)
        {
            fogCenterLoc = GetShaderLocation(fogRenderShader, "circleCenter");
            fogRadiusLoc = GetShaderLocation(fogRenderShader, "circleRadius");
            TraceLog(LOG_INFO, "MINIMAP: fog_render.fs loaded (centerLoc=%d radiusLoc=%d)",
                     fogCenterLoc, fogRadiusLoc);
        }
        else
        {
            TraceLog(LOG_WARNING, "MINIMAP: fog_render.fs failed to load — fog disabled");
            fogCenterLoc = -1;
            fogRadiusLoc = -1;
        }
    }

    CalculateLayout();
    BuildGridTexture();

    initialized = true;
    UpdateView();
}

void MinimapScreen::Shutdown()
{
    if (gridTexture.id > 0)
        UnloadTexture(gridTexture);
    if (fogRT.id > 0)
        UnloadRenderTexture(fogRT);
    if (bgArtwork.id > 0)
        UnloadTexture(bgArtwork);
    if (fogRenderShader.id > 0)
    {
        UnloadShader(fogRenderShader);
        fogRenderShader = {0};
    }

    gridTexture = {0};
    fogRT = {0};
    bgArtwork   = {0};
    fogCenterLoc = -1;
    fogRadiusLoc = -1;
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
    UpdateView();
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

    Color fogColor = {FOG_COLOR_BEIGE};
    int tilePx = MINIMAP_TILE_PX;

    BeginTextureMode(fogRT);
    ClearBackground(BLANK);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            FogState state = (FogState)g_Minimap.fog[y * w + x];
            Color c;
            switch (state)
            {
                case FogState::UNEXPLORED: c = fogColor; break;
                case FogState::VISIBLE:    c = Fade(fogColor, 0.4f); break;
                case FogState::EXPLORED:   c = Fade(fogColor, 0.55f); break;
            }
            DrawRectangle(x * tilePx, y * tilePx, tilePx, tilePx, c);
        }
    }

    EndTextureMode();
}

void MinimapScreen::DrawFogLayer() const
{
    if (fogRT.id <= 0) return;

    int tilePx = MINIMAP_TILE_PX;
    int w = g_Minimap.gridWidth;
    int h = g_Minimap.gridHeight;

    if (fogRenderShader.id > 0)
    {
        Vector2 pc = PlayerInstance.GetCenter();
        float fogH = (float)h * tilePx;
        // Circle center dalam fogRT pixel coords (Y-flip buat RenderTexture2D)
        Vector2 center = {
            pc.x / (float)FRAME_SIZE * tilePx,
            fogH - (pc.y / (float)FRAME_SIZE * tilePx)
        };
        float radius = (float)MINIMAP_REVEAL_RADIUS * tilePx;

        SetShaderValue(fogRenderShader, fogCenterLoc, &center, SHADER_UNIFORM_VEC2);
        SetShaderValue(fogRenderShader, fogRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(fogRenderShader);
    }

    // Source: fogRT.texture — RenderTexture2D, Y-flip via negative height
    Rectangle src = {0, 0,
                     (float)w * tilePx,
                     (float)-h * tilePx};
    Rectangle dst = {viewRect.x + panOffset.x,
                     viewRect.y + panOffset.y,
                     (float)w * MINIMAP_PANEL_SCALE,
                     (float)h * MINIMAP_PANEL_SCALE};

    DrawTexturePro(fogRT.texture, src, dst, {0, 0}, 0.0f, WHITE);

    if (fogRenderShader.id > 0)
        EndShaderMode();
}

/*==============================================================================
 * MinimapScreen - Player Marker
 *==============================================================================*/

void MinimapScreen::DrawPlayerMarker() const
{
    Vector2 playerCenter = PlayerInstance.GetCenter();

    // Posisi marker kontinu (sama kek UpdateView) biar gak jitter
    float markX = playerCenter.x / (float)FRAME_SIZE * MINIMAP_PANEL_SCALE;
    float markY = playerCenter.y / (float)FRAME_SIZE * MINIMAP_PANEL_SCALE;

    float sx = viewRect.x + panOffset.x + markX;
    float sy = viewRect.y + panOffset.y + markY;

    // Marker radius — pakai konstanta independen
    float innerR = MINIMAP_MARKER_RADIUS;
    float outerR = innerR * 1.5f;
    DrawCircle((int)sx, (int)sy, innerR, {100, 255, 100, 255});
    DrawCircle((int)sx, (int)sy, outerR, {100, 255, 100, 80});  // outer glow
}

/*==============================================================================
 * MinimapScreen - View (Auto-Follow Player)
 *==============================================================================*/

void MinimapScreen::UpdateView()
{
    if (!initialized)
        return;

    float scaledW = (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE;
    float scaledH = (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE;

    Vector2 playerPos = PlayerInstance.GetPosition();

    // Continuous (sub-tile) player position in minimap screen-pixel space
    // — sama kaya main camera follow: langsung pake pixel position, bukan tile grid
    float playerCenterX = (playerPos.x + FRAME_SIZE / 2.0f) / (float)FRAME_SIZE
                          * MINIMAP_PANEL_SCALE;
    float playerCenterY = (playerPos.y + FRAME_SIZE / 2.0f) / (float)FRAME_SIZE
                          * MINIMAP_PANEL_SCALE;

    if (scaledW > viewRect.width)
    {
        panOffset.x = (viewRect.width / 2.0f) - playerCenterX;
        panOffset.x = std::clamp(panOffset.x, viewRect.width - scaledW, 0.0f);
    }
    else
    {
        panOffset.x = (viewRect.width - scaledW) / 2.0f;
    }

    if (scaledH > viewRect.height)
    {
        panOffset.y = (viewRect.height / 2.0f) - playerCenterY;
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
        followPlayer = false;
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
 * MinimapScreen - Legend
 *==============================================================================*/

void MinimapScreen::DrawLegend() const
{
    Font font = GetOrLoad(FontId::MINIMAP_UI);

    // 1. Close hint — centered below panel
    {
        const char *closeKey = keybindManager.GetKeyDisplayName(TOGGLE_MAP);
        char closeBuf[64];
        snprintf(closeBuf, sizeof(closeBuf), "Press '%s' to Close", closeKey);
        float drawW = (float)bgArtwork.width  * 1.0f;
        float drawH = (float)bgArtwork.height * 1.0f;
        float ax = ((float)GameScreenWidth  - drawW) / 2.0f;
        float ay = ((float)GameScreenHeight - drawH) / 2.0f;

        int closeSz = 24;
        Vector2 textSz = MeasureTextEx(font, closeBuf, (float)closeSz, 0);
        float tx = ((float)GameScreenWidth - textSz.x) / 2.0f;
        float ty = ay + drawH - 50.0f;  
        DrawTextEx(font, closeBuf, {tx, ty}, (float)closeSz, 0, WHITE);
    }

    // 2. Drag / Center hints — bottom-right corner (kaya DrawMergeSplitLegend)
    {
        int hintSz = 18;
        const char *dragHint  = "[Left-Click Drag] Pan";
        const char *centerHint = "[Right-Click] Center to Player";

        Vector2 dragSz   = MeasureTextEx(font, dragHint, (float)hintSz, 0);
        Vector2 centerSz = MeasureTextEx(font, centerHint, (float)hintSz, 0);

        const float rightPad = 12.0f;
        float rightEdge = (float)GameScreenWidth - rightPad;
        float baseY = (float)GameScreenHeight - 30.0f;

        float dragX   = rightEdge - dragSz.x;
        float dragY   = baseY - dragSz.y;
        float centerX = rightEdge - centerSz.x;
        float centerY = dragY - 4.0f - centerSz.y;

        float legendY = centerY;
        float legendH = (dragY + dragSz.y) - centerY;
        float legendW = (dragSz.x > centerSz.x) ? dragSz.x : centerSz.x;

        DrawRectangleRounded(
            {rightEdge - legendW - 8, legendY - 4, legendW + 16, legendH + 8},
            0.4f, 8, ColorAlpha(BLACK, 0.8f));

        DrawTextEx(font, centerHint, {centerX, centerY}, (float)hintSz, 0, WHITE);
        DrawTextEx(font, dragHint,   {dragX, dragY},     (float)hintSz, 0, WHITE);
    }
}

/*==============================================================================
 * MinimapScreen - Update / Draw
 *==============================================================================*/

void MinimapScreen::Update(GameState* state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active || !initialized)
        return;

    (void)state;

    // Auto-follow player (dijeda pas manual drag)
    if (followPlayer && !isDragging)
        UpdateView();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        followPlayer = true;
        UpdateView();
    }

    HandlePan(mousePosition, mouseClicked);
    UpdateMinimapFog((int)PlayerInstance.GetCenter().x, (int)PlayerInstance.GetCenter().y, FRAME_SIZE);

    // Render fog ke fogRT — di Update (bukan Draw) biar gak corrupt state render
    RenderFogLayer();
}

void MinimapScreen::Draw(Vector2 mousePosition)
{
    if (!active || !initialized)
        return;

    (void)mousePosition;

    // Full screen black tint
    DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.7f));

    // Background artwork (decorative, behind panel)
    if (bgArtwork.id > 0)
    {
        float artScale = 1.1f;
        float drawW = (float)bgArtwork.width  * artScale;
        float drawH = (float)bgArtwork.height * artScale;
        float ax = ((float)GameScreenWidth  - drawW) / 2.0f;
        float ay = ((float)GameScreenHeight - drawH) / 2.0f;
        DrawTexturePro(bgArtwork,
                       {0, 0, (float)bgArtwork.width, (float)bgArtwork.height},
                       {ax, ay, drawW, drawH},
                       {0, 0}, 0.0f, WHITE);
    }

    BeginScissorMode((int)viewRect.x, (int)viewRect.y,
                     (int)viewRect.width, (int)viewRect.height);

    // Grid texture (regular Texture2D — positive height, bukan render texture)
    Rectangle srcGrid = {0, 0,
                         (float)(g_Minimap.gridWidth  * MINIMAP_TILE_PX),
                         (float)(g_Minimap.gridHeight * MINIMAP_TILE_PX)};
    Rectangle dst = {viewRect.x + panOffset.x,
                     viewRect.y + panOffset.y,
                     (float)g_Minimap.gridWidth  * MINIMAP_PANEL_SCALE,
                     (float)g_Minimap.gridHeight * MINIMAP_PANEL_SCALE};

    DrawTexturePro(gridTexture, srcGrid, dst, {0, 0}, 0.0f, WHITE);
    DrawFogLayer();
    DrawPlayerMarker();

    EndScissorMode();

    // Legend
    DrawLegend();
}

/*==============================================================================
 * MinimapSystem — Public Namespace Implementation
 *==============================================================================*/

void MinimapSystem::InitWithMap()
{
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
    ResetMinimapFog();

    // Restore fog dari cache kalo ada data untuk map ini
    // sizeof dicek biar aman kalo dimensi map berubah antar session
    const char* mapPath = GetCurrentMapPath();
    if (mapPath && mapPath[0] != '\0')
    {
        auto it = g_Minimap.fogCache.find(std::string(mapPath));
        if (it != g_Minimap.fogCache.end() && it->second.size() == g_Minimap.fog.size())
            g_Minimap.fog = it->second;
    }

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
    UpdateMinimapFog((int)PlayerInstance.GetCenter().x, (int)PlayerInstance.GetCenter().y, FRAME_SIZE);

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
