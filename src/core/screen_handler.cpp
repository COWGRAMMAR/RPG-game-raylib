/**
 * @file screen.cpp
 * @brief Implementasi Screen & GameState Management Module
 *
 * Handle virtual screen rendering, window scaling, dan game loop entry points.
 * Semua rendering dan update logic masuk lewat fungsi-fungsi di sini.
 *
 * Arsitektur rendering:
 * - Game dirender ke RenderTexture2D virtual (1280x720)
 * - Texture virtual di-scale ke window asli sambil jaga aspect ratio
 * - Sisi yang tidak terpakai diisi black bar (letterbox)
 *
 * Urutan init yang benar:
 * InitScreen() → InitMap() → InitAll() → masuk game loop
 */

#include "screen.h"
#include "map.h"
#include "player.h"
#include "item.h"
#include "inventory.h"
#include "animation.h"
#include "fonts.h"
#include "enemy.h"
#include "enemy_ai.h"
#include "entities.h"
#include "mapLogic.h"
#include "effects.h"
#include "game_debug.h"
#include "pauseMenu.h"
#include "combat.h"
#include "interaction.h"
#include "input.h"
#include <cstdio>
#include "enemy_ai.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/raylib/include/raymath.h"
#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "hud.h"
#include "propsbehavior.h"
#include "seedmanager.h"
#include "worldgenio.h"
#include "worldgenenartion.h"

/*==============================================================================
 * External Variables & Macros
 *==============================================================================*/

/** @brief Instance global game state */
GameState *gState;

extern PauseMenu pauseMenu;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*==============================================================================
 * Constants
 *==============================================================================*/

/** @brief Skala monitor untuk UI */
const float ScaleMultiplierMonitor = 0.7F;
/** @brief Skala minimum monitor */
const float ScaleMinMultiplierMonitor = 0.4F;

extern const int GameScreenWidth = 1280;
extern const int GameScreenHeight = 720;

/*==============================================================================
 * Initialization
 *==============================================================================*/

/**
 * @brief Inisialisasi semua entity dan camera di awal game
 *
 * Wajib dipanggil setelah InitMap() karena player butuh data map untuk spawn.
 * Camera langsung di-set ke posisi spawn player setelah init.
 */
void InitAll()
{
    // Bersihkan seluruh entitas sebelum inisialisasi ulang
    Entities::Clear();

    // init player — spawn point dibaca otomatis dari object layer Tiled
    PlayerInstance.Init(gState, SPAWN_OBJECT_NAME);

    // init enemy
    InitEnemy();

    // Testing spawn item
    InitItems();

    // set camera ke tengah posisi spawn player
    Vector2 spawnPos = PlayerInstance.GetPosition();
    camera.target = {spawnPos.x + (FRAME_SIZE / 2.0F), spawnPos.y + (FRAME_SIZE / 2.0F)};
    camera.offset = {(float)(GameScreenWidth / 2), (float)(GameScreenHeight / 2)};
    camera.rotation = 0;
    camera.zoom = 1.0F;

    // Daftarkan player ke sistem entitas agar diupdate & dirender otomatis (Index 0)
    Entities::Add(&PlayerInstance);

    SpawnObject();
    RebuildObstacleCache();
    globalFlowField.Invalidate(); // nanti diganti kalo nambah method ai nya
    // Spawn musuh dari map aktif
    SpawnEnemiesFromMap();

    // Capture spawn pos start room buat revive
    TiledHelperFunction.TryGetObjectPositionByName(SPAWN_OBJECT_NAME, gState->startSpawnPos);

    // Cache enemy & item state buat restart
    const char *mapPath = GetCurrentMapPath();
    if (mapPath)
    {
        std::string cachePath = std::string(mapPath) + ".cache";
        SaveEnemiesForMap(cachePath);
        SaveItemsForMapDir(cachePath);
    }
}

/**
 * @brief Fungsi pembantu untuk mengubah string menjadi lowercase
 */
static std::string ToLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return str;
}

/**
 * @brief Inisialisasi window, audio, dan render texture virtual
 *
 * Urutan init internal:
 * 1. Buat window resizable ukuran 70% monitor
 * 2. Hitung scale multiplier berdasarkan ukuran monitor
 * 3. Buat RenderTexture2D 1280x720 sebagai layar virtual
 * 4. Set FPS target ke 60
 *
 * @return GameState yang sudah diinisialisasi, siap dipakai game loop
 */
GameState InitScreen()
{
    GameState state = {{0}};

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Dungeon Game");
    SetExitKey(0);  // ESC handled by keybindManager (pause toggle), not by raylib quit
    InitAudioDevice();

    state.WindowScreenWidth = (int)(GetMonitorWidth(0) * ScaleMultiplierMonitor);
    state.WindowScreenHeight = (int)(GetMonitorHeight(0) * ScaleMultiplierMonitor);
    state.ScaleMultiplier = MIN(
        (float)state.WindowScreenWidth / GameScreenWidth,
        (float)state.WindowScreenHeight / GameScreenHeight);

    SetWindowSize(state.WindowScreenWidth, state.WindowScreenHeight);
    SetWindowMinSize(
        (int)(GetMonitorWidth(0) * ScaleMinMultiplierMonitor),
        (int)(GetMonitorHeight(0) * ScaleMinMultiplierMonitor));

    state.Dungeon = LoadRenderTexture(GameScreenWidth, GameScreenHeight);
    SetTextureFilter(state.Dungeon.texture, TEXTURE_FILTER_BILINEAR);

    const int FPS = 60;
    SetTargetFPS(FPS);

    state.currentScreen = MAIN_MENU;
    state.showFPS = false;
    state.isSwitchingMap = false;
    state.isGoingBack = false;
    state.pendingMapPath.clear();
    state.pendingDoorName.clear();

    // Create save directories at startup
    std::filesystem::create_directories("saves/manual");
    std::filesystem::create_directories("saves/autosave");

    return state;
}

/*==============================================================================
 * Update
 *==============================================================================*/

/**
 * @brief Update ukuran window dan scale multiplier tiap frame
 */
void UpdateGame(GameState *state)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w != state->WindowScreenWidth || h != state->WindowScreenHeight)
    {
        state->WindowScreenWidth = w;
        state->WindowScreenHeight = h;
        state->ScaleMultiplier = MIN(
            (float)w / GameScreenWidth,
            (float)h / GameScreenHeight);
    }
}

/**
 * @brief Entry point update logic game — dipanggil sekali per frame
 *
 * Menggunakan Entities::Update() sebagai entry point utama update semua entity.
 * Player.Update() dan Enemy.Update() dipanggil otomatis dari situ.
 */
void UpdateLogicAll()
{
    // update flow field sebelum enemy di-update
    if (tilesonMap)
        globalFlowField.Update(PlayerInstance.GetPosition(), tilesonMap->width, tilesonMap->height);

    if (!spawnFlowFieldRebuildQueue.empty() && tilesonMap)
    {
        int id = spawnFlowFieldRebuildQueue.front();
        spawnFlowFieldRebuildQueue.pop();
        auto &entry = spawnFlowFields[id];
        entry.field.Build(entry.spawnPos, tilesonMap->width, tilesonMap->height, FLOW_FIELD_RETURN_RADIUS);
    }

    RebuildSpatialHash(Entities::GetEnemyRegistry());

    auto &enemyReg = Entities::GetEnemyRegistry();
    for (int i = 0; i < (int)enemyReg.size(); i++)
    {
        if (!enemyReg[i]->IsActive)
            continue;
        Vector2 sep = CalcSeparationForce(i, enemyReg);
        // lerp force lama ke force baru
        enemyReg[i]->SeparationForce.x = Lerp(enemyReg[i]->SeparationForce.x, sep.x, SEPARATION_FORCE_MAGNITUDE);
        enemyReg[i]->SeparationForce.y = Lerp(enemyReg[i]->SeparationForce.y, sep.y, SEPARATION_FORCE_MAGNITUDE);

        Vector2 newPos = {
            enemyReg[i]->Position.x + enemyReg[i]->SeparationForce.x * Time::DELTA_TIME,
            enemyReg[i]->Position.y + enemyReg[i]->SeparationForce.y * Time::DELTA_TIME};

        if (IsPositionSafe(newPos, enemyReg[i]->HitboxWidth, enemyReg[i]->HitboxHeight,
                           enemyReg[i]->HitboxOffsetX, enemyReg[i]->HitboxOffsetY))
        {
            enemyReg[i]->Position = newPos;
        }
    }

    // Update semua entity (Player + semua Enemy) via Entities registry
    Entities::Update();

    // Handle pending map transitions dari Interaction namespace
    Interaction::ExecutePendingTransitions(PlayerInstance);

    // Deteksi FINISH cell untuk stage transition (hanya di worldgen stage)
    // Guard isSwitchingMap — cegah double trigger dari grid + door detection
    if (!gState->isSwitchingMap && !gState->isGoingBack)
    {
        const char *mapPath = GetCurrentMapPath();
        if (mapPath && strstr(mapPath, "worldseed/save_") != nullptr)
        {
            if (InputInstance.IsInteract())
            {
                Vector2 playerCenter = PlayerInstance.GetCenter();
                CellType cellType = GetCellTypeAtWorldPos(playerCenter);
                if (cellType == CELL_FINISH)
                {
                    WorldgenIO::NextStage();
                }

            }
        }
    }

    // Update Effects (Popups, Logs, etc)
    Effects::Update(Time::DELTA_TIME);
    spikeManager.Update(Time::DELTA_TIME, PlayerInstance.GetHitbox(), &PlayerInstance);
    bombManager.Update(Time::DELTA_TIME, PlayerInstance.GetHitbox(), &PlayerInstance);
    crateManager.Update();
    barrierManager.Update();

    // Update item magnet/pickup
    Vector2 center = PlayerInstance.GetCenter();
    Rectangle pHitbox = {
        center.x - PlayerInstance.GetHitboxWidth() / 2,
        center.y - PlayerInstance.GetHitboxHeight() / 2,
        PlayerInstance.GetHitboxWidth(),
        PlayerInstance.GetHitboxHeight()};

    itemRender.Update(itemData.activeItems, center, pHitbox, PlayerInstance.GetMagnetRadius(), PlayerInstance.GetItemSpeed());

    // Pickup scan setelah magnet update
    for (auto &item : itemData.activeItems)
    {
        if (item.isPickedUp && !item.isAdded)
        {
            TraceLog(LOG_INFO, "PICKUP: trying to add definitionId=%d", item.definitionId);
            if (Inventory::AddToInventory(PlayerInstance, item))
            {
                TraceLog(LOG_INFO, "PICKUP: added to inventory");
                item.isAdded = true;

                const ItemDefinition &def = itemDefs.GetById(item.definitionId);
                std::string logMsg = def.name;
                if (item.amount > 1)
                {
                    logMsg += " x" + std::to_string(item.amount);
                }
                Effects::AddLog(logMsg.c_str());
            }
            else
            {
                TraceLog(LOG_INFO, "PICKUP: inventory full");
                item.isPickedUp = false; // balik ke world

                static float lastInventoryFullTime = 0.0f;
                float currentTime = (float)GetTime();
                if (currentTime - lastInventoryFullTime > 2.0f)
                {
                    Effects::AddLog("Inventori Penuh");
                    lastInventoryFullTime = currentTime;
                }
            }
        }
    }
}

/*==============================================================================
 * Rendering
 *==============================================================================*/

/**
 * @brief Entry point render — semua yang terlihat di layar lewat sini
 *
 * Urutan layer render (urutan penting, jangan diubah sembarangan):
 * 1. Map — tile-based, tidak kena transform camera
 * 2. Entities, Items, Effects & world overlay — dalam BeginMode2D (world space)
 * 3. Debug panels — screen space, di luar BeginMode2D
 * 4. UI overlay — HUD, pause menu, dll
 */
void DrawRenderTexture(GameState *state)
{
    BeginTextureMode(state->Dungeon);
    ClearBackground(BLACK);

    // layer 1: tile map
    RenderMap();

    // layer 2: entities, items, effects & world overlay (world space)
    BeginMode2D(camera);
    Rectangle viewRect = GetVisibleWorldRect();
    RenderTileProps(viewRect);
    itemRender.RenderAll(itemData.activeItems, viewRect);
    Entities::Render(viewRect);
    Effects::Draw();
    DebugInstance.DrawWorldOverlay();
    EndMode2D();

    // layer 3: debug panels (screen space)
    DebugInstance.Toggle();
    DebugInstance.Draw();

    // layer 4: UI overlay
    DrawUIOverlay(state);

    EndTextureMode();
}

/**
 * @brief Render UI elements ke virtual screen
 */
void DrawUIOverlay(GameState *state)
{
    DrawPlayerHUD();

    // 2. FPS Counter (if enabled)
    if (state->showFPS)
    {
        int fps = GetFPS();
        char fpsText[16];
        snprintf(fpsText, sizeof(fpsText), "FPS: %d", fps);
        DrawText(fpsText, 10, 10, 20, GREEN);
    }

    // 3. Sign dialog overlay (placeholder UI)
    DrawSignDialog();

    // 4. Menus
    if (pauseMenu.IsActive())
    {
        Vector2 mousePos = GetVirtualMousePosition(state);
        pauseMenu.Draw(mousePos);
    }
}

/**
 * @brief Scale dan render layar virtual ke window asli
 */
void DrawRenderWindows(GameState *state)
{
    float offsetX = (state->WindowScreenWidth - ((float)GameScreenWidth * state->ScaleMultiplier)) * 0.5F;
    float offsetY = (state->WindowScreenHeight - ((float)GameScreenHeight * state->ScaleMultiplier)) * 0.5F;

    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexturePro(
        state->Dungeon.texture,
        (Rectangle){0, 0, (float)GameScreenWidth, -(float)GameScreenHeight},
        (Rectangle){offsetX, offsetY, (float)GameScreenWidth * state->ScaleMultiplier, (float)GameScreenHeight * state->ScaleMultiplier},
        (Vector2){0, 0},
        0.0F,
        WHITE);

    EndDrawing();
}

/*==============================================================================
 * Utilities
 *==============================================================================*/

/**
 * @brief Konversi posisi mouse dari window space ke virtual screen space
 */
Vector2 GetVirtualMousePosition(GameState *state)
{
    Vector2 mouse = GetMousePosition();
    Vector2 virtualMouse = {0, 0};

    virtualMouse.x = (mouse.x - ((state->WindowScreenWidth - (GameScreenWidth * state->ScaleMultiplier)) * 0.5F)) / state->ScaleMultiplier;
    virtualMouse.y = (mouse.y - ((state->WindowScreenHeight - (GameScreenHeight * state->ScaleMultiplier)) * 0.5F)) / state->ScaleMultiplier;

    return Vector2Clamp(virtualMouse, (Vector2){0, 0}, (Vector2){(float)GameScreenWidth, (float)GameScreenHeight});
}

/*==============================================================================
 * Cleanup
 *==============================================================================*/

/**
 * @brief Bersihin semua resource sebelum game ditutup
 */
void GameShutDown(GameState *state)
{
    CloseTextures();
    UnloadFonts();

    Entities::Shutdown();
    UnloadMap();
    UnloadRenderTexture(state->Dungeon);

    CloseAudioDevice();
    CloseWindow();
}

/*==============================================================================
 * Window & Video Settings Functions
 *==============================================================================*/

/** @brief Toggle fullscreen mode */
void ToggleFullscreenMode(void)
{
    if (IsWindowFullscreen())
    {
        ToggleFullscreen();
    }
    else
    {
        ToggleFullscreen();
    }
}

/** @brief Set resolusi window */
void SetResolution(int width, int height)
{
    SetWindowSize(width, height);
}

/** @brief Get resolusi window saat ini */
Rectangle GetCurrentResolution(void)
{
    Rectangle res = {0};
    res.width = static_cast<float>(GetScreenWidth());
    res.height = static_cast<float>(GetScreenHeight());
    return res;
}

/** @brief Get resolusi monitor utama */
Rectangle GetMonitorResolution(void)
{
    Rectangle res = {0};
    res.width = static_cast<float>(GetMonitorWidth(0));
    res.height = static_cast<float>(GetMonitorHeight(0));
    return res;
}

/** @brief Cek apakah fullscreen */
bool IsFullscreen(void)
{
    return IsWindowFullscreen();
}

/*==============================================================================
 * Shared Background
 *==============================================================================*/

/**
 * @brief Gambar background gradient untuk layar non-gameplay (main menu, loading screen).
 *
 * Dipanggil oleh RenderMainMenuToVirtualScreen() dan RenderLoadingScreen().
 * Begitu animated BG tersedia, cukup ganti implementasi di sini saja.
 */
void DrawMenuBackground(void)
{
    DrawRectangleGradientV(
        0, 0,
        GameScreenWidth, GameScreenHeight,
        {36, 28, 58, 255},   // top: muted dark purple-blue
        {5, 5, 15, 255}      // bottom: near-black
    );
}
