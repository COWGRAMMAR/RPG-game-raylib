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
#include "../../include/rendering/fonts.h"
#include <algorithm>
#include <cstring>
#include <cctype>
#include "../../lib/raylib/include/raylib.h"

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

/**
 * @brief UpdateLoadingScreen()
 * Update logic loading screen dan sequence loading asset.
 * @param state Pointer ke GameState
 * @details Load asset per stage, skip jika assetsLoaded sudah true.
 *          Juga menangani map switch dengan stages terpisah.
 */
void UpdateLoadingScreen(GameState *state)
{
    UpdateGame(state);

    if (state->loadingComplete)
    {
        return;
    }

    /*==============================================================================
     * Map Switch Loading - handle transisi map dengan loading screen
     *==============================================================================*/
    if (state->isSwitchingMap || state->isGoingBack)
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

            /**
             * @brief Worldgen map-switch: run worldgen stage and load runtime state
             * Block ini jalan saat map-switch (bukan Load Game) ketika masuk ke
             * worldgen stage. Panggil RunWorldgenStage() untuk generate stage map,
             * lalu LoadRuntimeState() untuk restore runtime state per-stage.
             * LoadRuntimeState() overwrite DeadEntities dengan subset stage ini —
             * intentional. Clear worldgen pending flag setelah load agar
             * RestoreDeadEntities selanjutnya jalan normal.
             */
            if (!isBack && state->pendingMapPath.find("worldseed/save_" + std::to_string(g_ActiveSaveSlot)) != std::string::npos)
            {
                int stageIdx = g_SeedManager.GetCurrentStage();
                uint64_t seed = g_SeedManager.GetSeed(stageIdx);
                RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);
                WorldgenIO::LoadRuntimeState(g_SeedManager.GetCurrentStage());
                // Worldgen runtime state is now active — clear the pending flag so
                // future calls to RestoreDeadEntities (e.g. after returning to overworld)
                // restore from the main save file as normal.
                SetWorldgenPending(false);
            }
            else
            {
                BuildMapObjectIndex();
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
            // Re-init player berdasarkan target door di map baru
            PlayerInstance.Init(gState, state->pendingDoorName.c_str());

            // Capture start room spawn position untuk revive
            TiledHelperFunction.TryGetObjectPositionByName(SPAWN_OBJECT_NAME, gState->startSpawnPos);

            // Bersihkan entitas map sebelumnya dan add player
            Entities::Clear();
            Entities::Add(&PlayerInstance);

            // Load musuh dari save per-map, atau spawn baru jika tak ada
            if (!LoadEnemiesForMap(state->pendingMapPath))
            {
                SpawnEnemiesFromMap();
            }

            // Load items from filesystem persistence
            if (!LoadItemsForMapDir(state->pendingMapPath))
            {
                SpawnItemWave();
            }

            // Capture cache untuk restart
            {
                const char *currentPath = GetCurrentMapPath();
                if (currentPath)
                {
                    std::string cachePath = std::string(currentPath) + ".cache";
                    SaveEnemiesForMap(cachePath, "saves/cache/enemies");
                    SaveItemsForMapDir(cachePath, "saves/cache/items");
                }
            }

            state->loadingStage++;
            state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
            break;

        case 3:
            TraceLog(LOG_INFO, "LOADING: [stage 4/4] Finalizing map switch");
            state->loadingText = "Finalizing map switch...";
            // Set camera ke spawn player
            Vector2 spawnPos = PlayerInstance.GetPosition();
            camera.target = {spawnPos.x + (FRAME_SIZE / 2.0F), spawnPos.y + (FRAME_SIZE / 2.0F)};
            camera.offset = {(float)(GameScreenWidth / 2), (float)(GameScreenHeight / 2)};
            camera.rotation = 0;
            camera.zoom = 1.0F;
            Movement::UpdateCamera(PlayerInstance);

            // Clear map switch state
            state->isSwitchingMap = false;
            state->isGoingBack = false;
            state->pendingMapPath.clear();
            state->pendingDoorName.clear();

            TraceLog(LOG_INFO, "LOADING: Map switch complete, player at (%.2f, %.2f)", PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y);
            WriteAutosave("quick.json");
            state->loadingComplete = true;
            state->loadingProgress = 100.0F;
            state->loadingText = "Map loaded!";
            state->currentScreen = PLAY;
            break;
        }
        return;
    }

    /*==============================================================================
     * Jika asset sudah dimuat, skip loading stages
     *==============================================================================*/
    if (state->assetsLoaded)
    {
        TraceLog(LOG_INFO, "LOADING: Fast path (assets already loaded)");

        state->loadingStage = TOTAL_LOADING_STAGES;
        state->loadingProgress = 100.0F;
        state->loadingComplete = true;
        state->currentScreen = PLAY;

        // Free previous map allocation before loading the correct one
        UnloadMap();

        if (HasSavedState())
        {
            if (!savedMapState.mapPath.empty())
            {
                LoadMap(savedMapState.mapPath.c_str());
                SetCurrentMapPath(savedMapState.mapPath.c_str());
                BuildMapObjectIndex();

                // Worldgen: regenerate layout + runtime state like normal switch path
                if (savedMapState.mapPath.find("worldseed/save_" + std::to_string(g_ActiveSaveSlot)) != std::string::npos)
                {
                    int stageIdx = g_SeedManager.GetCurrentStage();
                    uint64_t seed = g_SeedManager.GetSeed(stageIdx);
                    RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);
                    WorldgenIO::LoadRuntimeState(stageIdx);
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

        /**
         * @brief Worldgen save detection (fast path)
         * Jika savedMapState.mapPath mengandung "worldseed/save_", save dibuat
         * saat mid-worldgen. Set pending flag agar RestoreDeadEntities() di bawah
         * di-skip — WorldgenIO's LoadRuntimeState akan handle dead entities
         * dari per-stage runtime data saat worldgen switch nanti.
         */
        if (HasSavedState() && savedMapState.mapPath.find("worldseed/save_" + std::to_string(g_ActiveSaveSlot)) != std::string::npos)
            SetWorldgenPending(true);

        // Restore dead entities BEFORE InitAll to prevent dead enemies respawning.
        // Skip if save points to a worldgen map -- WorldgenIO's LoadRuntimeState
        // will set dead entities from per-stage runtime data during the next switch.
        if (HasSavedState() && !IsWorldgenPending())
            RestoreDeadEntities();
        // Init first, then restore saved state - order matters!
        // InitAll() sets position to spawn, then RestoreGameState overwrites it
        InitAll();
        InitFonts();
        if (HasSavedState())
        {
            RestoreGameState(state);
            TraceLog(LOG_INFO, "LOADING: after RestoreGameState, player pos = (%.2f, %.2f)", PlayerInstance.GetPosition().x, PlayerInstance.GetPosition().y);
        }
        else
        {
            WriteAutosave("spawn.json");
        }
        // Safety net: deactivate any dead entities that survived spawn
        Entities::PruneDeadEntities();

        // Capture fresh cache so restart has correct initial state
        {
            const char *currentPath = GetCurrentMapPath();
            if (currentPath)
            {
                std::string cachePath = std::string(currentPath) + ".cache";
                SaveEnemiesForMap(cachePath, "saves/cache/enemies");
                SaveItemsForMapDir(cachePath, "saves/cache/items");
            }
        }

        InitMainMenu(state);
        return;
    }

    /*==============================================================================
     * Loading sequence untuk pertama kali
     *==============================================================================*/
    switch (state->loadingStage)
    {
    case 0:
        state->loadingText = "Loading game textures...";
        InitTextures();
        state->loadingStage++;
        state->loadingProgress = (float)state->loadingStage / TOTAL_LOADING_STAGES * 100.0F;
        break;

    case 1:
        state->loadingText = "Loading map data...";
        // Load saved map if resuming, otherwise default
        if (HasSavedState() && !savedMapState.mapPath.empty())
        {
            LoadMap(savedMapState.mapPath.c_str());
            SetCurrentMapPath(savedMapState.mapPath.c_str());
            BuildMapObjectIndex();
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
        state->loadingComplete = true;
        state->assetsLoaded = true;
        state->loadingText = "Loading complete!";
        state->currentScreen = PLAY;

        /**
         * @brief Worldgen save detection (initial load)
         * Jika savedMapState.mapPath mengandung "worldseed/save_", save dibuat
         * saat mid-worldgen. Set pending flag agar RestoreDeadEntities di bawah
         * di-skip — WorldgenIO's LoadRuntimeState akan handle dead entities
         * dari per-stage runtime data saat worldgen switch nanti.
         */
        if (HasSavedState() && savedMapState.mapPath.find("worldseed/save_" + std::to_string(g_ActiveSaveSlot)) != std::string::npos)
            SetWorldgenPending(true);

        // Restore dead entities BEFORE InitAll to prevent dead enemies respawning.
        // Skip if save points to a worldgen map -- WorldgenIO's LoadRuntimeState
        // will set dead entities from per-stage runtime data during the next switch.
        if (HasSavedState() && !IsWorldgenPending())
            RestoreDeadEntities();
        // Initialize everything first, then restore saved state
        InitAll();
        if (HasSavedState())
        {
            RestoreGameState(state);
        }
        else
        {
            WriteAutosave("spawn.json");
        }
        // Safety net: deactivate any dead entities that survived spawn
        Entities::PruneDeadEntities();
        InitMainMenu(state);
        break;
    }
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

    Vector2 textSize = MeasureTextEx(fontLoadingTitle, state->loadingText, 32, 2);
    float textX = (GameScreenWidth - textSize.x) / 2.0f;
    float textY = (float)(GameScreenHeight / 2) - textSize.y - 30.0f;
    DrawTextEx(fontLoadingTitle, state->loadingText, {textX, textY}, 32, 2, WHITE);

    // Smooth progress bar animation
    static float currentDisplayProgress = 0.0f;
    float dt = fminf(GetFrameTime(), 0.1f);
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
    Vector2 pctSize = MeasureTextEx(fontLoadingTitle, progressText.data(), 20, 1);
    float pctX = (GameScreenWidth - pctSize.x) / 2.0f;
    float pctY = (float)(GameScreenHeight / 2) + 50.0f;
    DrawTextEx(fontLoadingTitle, progressText.data(), {pctX, pctY}, 20, 1, WHITE);

    // Map name display
    std::string mapName = GetDisplayMapName();
    if (!mapName.empty()) {
        Vector2 mapSize = MeasureTextEx(fontLoadingTitle, mapName.c_str(), 18, 1);
        float mapX = (GameScreenWidth - mapSize.x) / 2.0f;
        float mapY = (float)(GameScreenHeight / 2) + 80.0f;
        DrawTextEx(fontLoadingTitle, mapName.c_str(), {mapX, mapY}, 18, 1, ColorAlpha(WHITE, 0.6f));
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
