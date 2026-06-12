/**
 * @file loading_screen.cpp
 * @brief Implementasi Modul Loading Screen
 *
 * Handle tampilan loading screen dan sequence loading asset game.
 * Asset dimuat sekali saja saat pertama kali Start Game, kemudian reuse.
 * Juga menangani transisi map dengan loading screen yang sama.
 */

#include "../../include/core/loading_screen.h"
#include "../../include/map/map.h"
#include "../../include/entities/player.h"
#include "../../include/entities/enemy.h"
#include "../../include/items/item.h"
#include "../../include/ui/mainMenu.h"
#include "../../include/core/game_state_saver.h"
#include "../../include/core/screen.h"
#include "../../include/entities/entities.h"
#include "../../include/map/propsbehavior.h"
#include "../../include/entities/enemy_ai.h"
#include "../../include/core/seedmanager.h"
#include "../../include/map/worldgenio.h"
#include "../../include/core/savemanager.h"
#include "../../include/rendering/fonts.h"
#include "../../include/systems/audioManager.h"
#include <algorithm>
#include <cstring>
#include <cctype>
#include "raylib.h"

/*==============================================================================
 * Konstanta Loading
 *==============================================================================*/

/** @brief Total stage loading untuk initial startup */
#define TOTAL_LOADING_STAGES 3

/** @brief Total stage loading untuk map switch */
#define MAP_SWITCH_STAGES 4

/*==============================================================================
 * Public Functions
 *==============================================================================*/

/**
 * @brief InitLoadingScreen()
 * Inisialisasi state loading screen.
 * @param state Pointer ke GameState
 * @details Reset loadingStage dan loadingProgress saat masuk LOADING state
 */
void InitLoadingScreen(GameState *state)
{
    state->enteredLoading = true;
    state->loadingStage = 0;
    state->loadingProgress = 0.0F;
    state->loadingComplete = false;

    if (state->isSwitchingMap)
    {
        state->loadingText = "Switching map...";
    }
    else if (state->assetsLoaded)
    {
        state->loadingText = "Loading saved state...";
    }
    else
    {
        state->loadingText = "Starting asset loading...";
    }
}

// Ekstrak index stage (0-based) dari map path worldgen, misal: stage_2.json -> 1
// Dipakai kalo ada 2 save di 1 worldgen slot: meta.json punya stage lebih tinggi,
// tapi save yang di-load butuh seed stage yang bener.
/*=== Extract Stage dari Map Path ===*/

static int ExtractStageFromPath(const std::string& mapPath)
{
    auto pos = mapPath.find("stage_");
    if (pos == std::string::npos) return 0;
    pos += 6;
    int num = 0;
    while (pos < mapPath.size() && isdigit((unsigned char)mapPath[pos]))
    {
        num = num * 10 + (mapPath[pos] - '0');
        pos++;
    }
    return (num > 0) ? num - 1 : 0;
}

/*=== Shared Worldgen Helpers ===*/

// LoadMeta + extract stage dari mapPath + RunWorldgen
// Dipanggil pas initial load / fast path (bukan map-switch — SeedManager udah bener)
static bool LoadWorldgenForSave(const std::string& mapPath, int worldgenSlot)
{
    TraceLog(LOG_INFO, "LoadWorldgenForSave: worldgenSlot=%d mapPath='%s'", worldgenSlot, mapPath.c_str());
    if (worldgenSlot < 0) { TraceLog(LOG_WARNING, "LoadWorldgenForSave: worldgenSlot < 0 — skipping"); return false; }
    std::string metaPath = WorldgenIO::GetMetaPath(worldgenSlot);
    TraceLog(LOG_INFO, "LoadWorldgenForSave: metaPath='%s'", metaPath.c_str());
    if (!g_SeedManager.LoadMeta(metaPath)) { TraceLog(LOG_WARNING, "LoadWorldgenForSave: LoadMeta failed for '%s'", metaPath.c_str()); return false; }
    // Stage dari mapPath (per-save), bukan meta.json (bisa outdated kalo 2 save
    // di worldgen slot sama punya stage berbeda)
    int stageIdx = ExtractStageFromPath(mapPath);
    TraceLog(LOG_INFO, "LoadWorldgenForSave: stageIdx=%d (from mapPath)", stageIdx);
    g_SeedManager.SetCurrentStage(stageIdx);
    uint64_t seed = g_SeedManager.GetSeed(stageIdx);

    // InitItems belum tentu dipanggil sebelum LoadWorldgenForSave
    // (misal di HandleInitialLoad — InitItems baru jalan di InitAll setelahnya).
    // Padahal RunWorldgen → SpawnItemWave butuh itemDefs terisi.
    // Pastikan item definitions di-load biar SpawnAll gak crash di GetById(-1).
    itemDefs.Load("assets/data/items.json");

    RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);

    TraceLog(LOG_INFO, "LoadWorldgenForSave: complete — stage=%d pos=(%.0f,%.0f)", stageIdx, PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y);
    return true;
}

/*=== Map-switch Mode ===*/

static void HandleMapSwitch(GameState* state)
{
    bool isBack = state->isGoingBack;

    switch (state->loadingStage)
    {
    case 0:
        TraceLog(LOG_INFO, "LOADING: [stage 1/4] %s", isBack ? "Returning to previous map" : "Unloading current map");
        state->loadingText = isBack ? "Returning to previous map..." : "Unloading current map...";
        UnloadMap();
        spawnFlowFields.clear();
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
        break;

    case 1:
        TraceLog(LOG_INFO, "LOADING: [stage 2/4] Loading map: %s", state->pendingMapPath.c_str());
        state->loadingText = isBack ? "Reloading previous map..." : "Loading new map...";
        LoadMap(state->pendingMapPath.c_str());

        // Update map path segera agar IsAlreadyDead() pakai path yang benar
        SetCurrentMapPath(state->pendingMapPath.c_str());

        // Worldgen map-switch: SeedManager currentStage udah bener dari NextStage/PrevStage
        if (!isBack && state->pendingMapPath.find("worldseed/save_") != std::string::npos)
        {
            int stageIdx = g_SeedManager.GetCurrentStage();
            uint64_t seed = g_SeedManager.GetSeed(stageIdx);
            RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);

        }
        else
        {
            BuildMapObjectIndex();

            // BUGFIX: Apply pre-spawn state (dead entities + consumed positions)
            // SEBELUM SpawnObject agar chest/bomb/crate yang sudah dikonsumsi
            // tidak spawn ulang, dan enemy mati tidak di-respawn
            GameSnapshot chkSnap;
            if (SaveManager::HasCheckpoint(state->pendingMapPath, g_ActiveSaveSlot))
            {
                chkSnap = SaveManager::LoadCheckpoint(state->pendingMapPath, g_ActiveSaveSlot);
                SaveManager::ApplyPreSpawn(chkSnap);
            }
        }
        SpawnObject();
        RebuildObstacleCache();
        globalFlowField.Invalidate();
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
        break;

    case 2:
        TraceLog(LOG_INFO, "LOADING: [stage 3/4] Initializing player and entities");
        state->loadingText = "Initializing player and entities...";
        PlayerInstance.Init(gState, state->pendingDoorName.c_str());
        TiledHelperFunction.TryGetObjectPositionByName(SPAWN_OBJECT_NAME, gState->startSpawnPos);
        Entities::Clear();
        Entities::Add(&PlayerInstance);

        // Coba load checkpoint & apply pre-spawn (dead entities)
        {
            GameSnapshot chkSnap;
            bool hasCheckpoint = SaveManager::HasCheckpoint(state->pendingMapPath, g_ActiveSaveSlot);
            if (hasCheckpoint)
            {
                chkSnap = SaveManager::LoadCheckpoint(state->pendingMapPath, g_ActiveSaveSlot);
                SaveManager::ApplyPreSpawn(chkSnap);
            }

            SpawnEnemiesFromMap();
            SpawnItemWave();

            // Apply checkpoint state on top of fresh spawn
            if (hasCheckpoint)
                SaveManager::ApplyCheckpointData(chkSnap);
        }

        // Save initial state untuk restart
        {
            GameSnapshot initial = SaveManager::CaptureSnapshot();
            SaveManager::SaveInitial(initial, g_ActiveSaveSlot);
        }

        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
        break;

    case 3:
        TraceLog(LOG_INFO, "LOADING: [stage 4/4] Finalizing map switch");
        state->loadingText = "Finalizing map switch...";
        {
            Vector2 spawnPos = PlayerInstance.GetPosition();
            camera.target = {spawnPos.x + (FRAME_SIZE / 2.0F), spawnPos.y + (FRAME_SIZE / 2.0F)};
            camera.offset = {(float)(GameScreenWidth / 2), (float)(GameScreenHeight / 2)};
            camera.rotation = 0;
            camera.zoom = 1.0F;
            Movement::UpdateCamera(PlayerInstance);
        }

        state->isSwitchingMap = false;
        state->isGoingBack = false;
        state->pendingMapPath.clear();
        state->pendingDoorName.clear();

        TraceLog(LOG_INFO, "LOADING: Map switch complete, player at (%.2f, %.2f)", PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y);
        SaveManager::SaveAutosave(g_ActiveSaveSlot);
        state->loadingComplete = true;
        state->loadingProgress = 100.0F;
        state->loadingText = "Map loaded!";
        state->currentScreen = PLAY;
        break;
    }
}

/*=== Fast Path Mode (assets already loaded) ===*/

static void HandleFastPath(GameState* state)
{
    TraceLog(LOG_INFO, "=== HandleFastPath: slot=%d mapPath='%s' worldgenSlot=%d hasSaved=%d ===",
        g_ActiveSaveSlot, savedMapState.mapPath.c_str(), savedPlayerState.worldgenSlot, HasSavedState());

    state->loadingStage = TOTAL_LOADING_STAGES;
    state->loadingProgress = 100.0F;
    state->loadingComplete = true;
    state->currentScreen = PLAY;

    UnloadMap();

    if (HasSavedState())
    {
        if (!savedMapState.mapPath.empty())
        {
            LoadMap(savedMapState.mapPath.c_str());
            SetCurrentMapPath(savedMapState.mapPath.c_str());
            BuildMapObjectIndex();

            if (savedMapState.mapPath.find("worldseed/save_") != std::string::npos)
            {
                TraceLog(LOG_INFO, "HandleFastPath: calling LoadWorldgenForSave(worldgenSlot=%d)", savedPlayerState.worldgenSlot);
                LoadWorldgenForSave(savedMapState.mapPath, savedPlayerState.worldgenSlot);
            }
        }
        else
        {
            InitMap();
        }
    }
    else
    {
        PlayerInstance.ResetForNewGame();
        InitMap();
    }

    // BUGFIX: Apply pre-spawn state (dead entities + consumed positions)
    // SEBELUM InitAll agar chest/bomb/crate yang sudah dikonsumsi
    // tidak spawn ulang (SpawnObject dipanggil via InitAll)
    {
        GameSnapshot snap;
        bool willApply = HasSavedState() && SaveManager::HasManual(g_ActiveSaveSlot);
        TraceLog(LOG_INFO, "HandleFastPath: ApplyPreSpawn decision hasSaved=%d hasManual=%d -> %s",
            HasSavedState(), SaveManager::HasManual(g_ActiveSaveSlot),
            willApply ? "YES" : "SKIP");
        if (willApply)
        {
            snap = SaveManager::LoadManual(g_ActiveSaveSlot);
            SaveManager::ApplyPreSpawn(snap);
        }
    }

    InitAll();
    if (HasSavedState())
    {
        RestoreGameState(state);
        TraceLog(LOG_INFO, "LOADING: after RestoreGameState, player pos = (%.2f, %.2f)", PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y);
    }
    else
    {
        SaveManager::SaveAutosave(g_ActiveSaveSlot);
    }

    Entities::PruneDeadEntities();

    // Save initial state untuk restart
    {
        GameSnapshot initial = SaveManager::CaptureSnapshot();
        SaveManager::SaveInitial(initial, g_ActiveSaveSlot);
    }

    InitMainMenu(state);
}

/*=== Initial Load Mode ===*/

static void HandleInitialLoad(GameState* state)
{
    switch (state->loadingStage)
    {
    case 0:
        TraceLog(LOG_INFO, "=== HandleInitialLoad stage 0: slot=%d assetsLoaded=%d hasSaved=%d mapPath='%s' worldgenSlot=%d ===",
            g_ActiveSaveSlot, state->assetsLoaded, HasSavedState(), savedMapState.mapPath.c_str(), savedPlayerState.worldgenSlot);
        state->loadingText = "Loading game textures...";
        InitTextures();
        AudioManager::InitSFX();
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / TOTAL_LOADING_STAGES * 100.0F;
        break;

    case 1:
        state->loadingText = "Loading map data...";
        TraceLog(LOG_INFO, "HandleInitialLoad stage 1: hasSaved=%d mapPath='%s'", HasSavedState(), savedMapState.mapPath.c_str());
        if (HasSavedState() && !savedMapState.mapPath.empty())
        {
            LoadMap(savedMapState.mapPath.c_str());

            if (tilesonMap != nullptr)
            {
                SetCurrentMapPath(savedMapState.mapPath.c_str());
                BuildMapObjectIndex();

                if (savedMapState.mapPath.find("worldseed/save_") != std::string::npos)
                {
                    TraceLog(LOG_INFO, "HandleInitialLoad: calling LoadWorldgenForSave(worldgenSlot=%d)", savedPlayerState.worldgenSlot);
                    if (LoadWorldgenForSave(savedMapState.mapPath, savedPlayerState.worldgenSlot))
                        BuildMapObjectIndex(); // Rebuild setelah worldgen ganti konten map
                    else
                        TraceLog(LOG_WARNING, "Worldgen meta not found for slot %d — save corrupted", savedPlayerState.worldgenSlot);
                }
            }
            else
            {
                TraceLog(LOG_WARNING, "LoadMap failed for '%s' — worldseed data may have been deleted", savedMapState.mapPath.c_str());
            }
        }
        else
        {
            InitMap();
        }
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / TOTAL_LOADING_STAGES * 100.0F;
        break;

    case 2:
        state->loadingText = "Finalizing game assets...";
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / TOTAL_LOADING_STAGES * 100.0F;
        break;

    default:
        // Crash guard: kalo LoadMap gagal (worldseed dir kehapus), tilesonMap null
        if (tilesonMap == nullptr)
        {
            state->loadingText = "Load failed — corrupted save, returning to menu...";
            state->loadingComplete = true;
            state->assetsLoaded = true;
            state->currentScreen = MAIN_MENU;
            InitMainMenu(state);
            break;
        }

        state->loadingComplete = true;
        state->assetsLoaded = true;
        state->loadingText = "Loading complete!";
        state->currentScreen = PLAY;

        // BUGFIX: Apply pre-spawn state SEBELUM InitAll
        {
            GameSnapshot snap;
            bool willApply = HasSavedState() && SaveManager::HasManual(g_ActiveSaveSlot);
            TraceLog(LOG_INFO, "HandleInitialLoad: ApplyPreSpawn hasSaved=%d hasManual=%d -> %s",
                HasSavedState(), SaveManager::HasManual(g_ActiveSaveSlot),
                willApply ? "YES" : "SKIP");
            if (willApply)
            {
                snap = SaveManager::LoadManual(g_ActiveSaveSlot);
                SaveManager::ApplyPreSpawn(snap);
            }
        }

        InitAll();
        if (HasSavedState())
            RestoreGameState(state);
        else
            SaveManager::SaveAutosave(g_ActiveSaveSlot);

        Entities::PruneDeadEntities();

        // Save initial state untuk restart
        {
            GameSnapshot initial = SaveManager::CaptureSnapshot();
            SaveManager::SaveInitial(initial, g_ActiveSaveSlot);
        }

        InitMainMenu(state);
        break;
    }
}

/*=== Main Dispatcher ===*/

/**
 * @brief UpdateLoadingScreen()
 * Update logic loading screen dan sequence loading asset.
 * @param state Pointer ke GameState
 * @details Load asset per stage, skip jika assetsLoaded sudah true.
 *          Juga menangani map switch dengan stages terpisah.
 *          Tiga mode: map-switch, fast-path (assets already loaded), initial load.
 */
void UpdateLoadingScreen(GameState *state)
{
    UpdateGame(state);

    if (state->loadingComplete)
        return;

    if (state->isSwitchingMap || state->isGoingBack)
    {
        HandleMapSwitch(state);
        return;
    }

    if (state->assetsLoaded)
    {
        HandleFastPath(state);
        return;
    }

    HandleInitialLoad(state);
}

/**
 * @brief GetDisplayMapName()
 * Mendapatkan display name dari map path saat ini.
 * @return std::string Nama yang ditampilkan, atau empty string jika belum ada map.
 */
static std::string GetDisplayMapName()
{
    const char* mapPath = GetCurrentMapPath();
    if (!mapPath || mapPath[0] == '\0')
    {
        mapPath = nullptr;
    }

    if (!mapPath)
    {
        return "";
    }

    // Handle worldgen paths
    if (strstr(mapPath, "worldseed") != nullptr)
    {
        return "Generating World...";
    }

    // Extract filename stem
    const char* filename = strrchr(mapPath, '/');
    if (!filename) filename = strrchr(mapPath, '\\');
    if (!filename) filename = mapPath; else filename++;

    // Remove .json extension
    std::string name(filename);
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    // Capitalize first letter
    if (!name.empty()) name[0] = (char)toupper((unsigned char)name[0]);

    return name;
}

/**
 * @brief RenderLoadingScreen()
 * Render loading screen ke layar virtual.
 * @param state Pointer ke GameState
 */
void RenderLoadingScreen(GameState *state)
{
    BeginTextureMode(state->Dungeon);
    DrawMenuBackground();

    Vector2 textSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), state->loadingText, 32, 2);
    float textX = (GameScreenWidth - textSize.x) / 2.0f;
    float textY = (float)(GameScreenHeight / 2) - textSize.y - 30.0f;
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), state->loadingText, {textX, textY}, 32, 2, WHITE);

    // Smooth progress bar animation
    static float currentDisplayProgress = 0.0f;
    float dt = fminf(Time::DELTA_TIME , 0.1f);
    float targetProgress = state->loadingProgress / 100.0f;

    if (state->loadingComplete) {
        currentDisplayProgress = targetProgress;
    } else {
        currentDisplayProgress += (targetProgress - currentDisplayProgress) * fminf(dt * 5.0f, 1.0f);
    }

    currentDisplayProgress = std::clamp(currentDisplayProgress, 0.0f, 1.0f);

    float barX = (float)(GameScreenWidth / 2) - 150.0f;
    float barY = (float)(GameScreenHeight / 2) + 20.0f;
    float barWidth = 300.0f;
    float barHeight = 20.0f;
    float animatedWidth = barWidth * currentDisplayProgress;

    // Draw progress bar (track + fill + border)
    DrawRectangleRounded({barX, barY, barWidth, barHeight}, 0.3f, 8, DARKGRAY);
    if (currentDisplayProgress > 0.0f) {
        DrawRectangleRounded({barX, barY, animatedWidth, barHeight}, 0.3f, 8, GREEN);
    }
    DrawRectangleRoundedLines({barX, barY, barWidth, barHeight}, 0.3f, 8, ColorAlpha(WHITE, 0.2f));

    std::array<char, 10> progressText;
    sprintf(progressText.data(), "%d%%", (int)state->loadingProgress);
    Vector2 pctSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), progressText.data(), 20, 1);
    float pctX = (GameScreenWidth - pctSize.x) / 2.0f;
    float pctY = (float)(GameScreenHeight / 2) + 50.0f;
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), progressText.data(), {pctX, pctY}, 20, 1, WHITE);

    // Map name display
    std::string mapName = GetDisplayMapName();
    if (!mapName.empty()) {
        Vector2 mapSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), mapName.c_str(), 18, 1);
        float mapX = (GameScreenWidth - mapSize.x) / 2.0f;
        float mapY = (float)(GameScreenHeight / 2) + 80.0f;
        DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), mapName.c_str(), {mapX, mapY}, 18, 1, ColorAlpha(WHITE, 0.6f));
    }

    EndTextureMode();
}

/**
 * @brief IsLoadingComplete()
 * Memeriksa apakah loading sudah selesai.
 * @param state Pointer ke GameState
 * @return true jika loading selesai
 */
bool IsLoadingComplete(GameState *state)
{
    return state->loadingComplete;
}
