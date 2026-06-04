/**
 * @file loading_screen.cpp
 * @brief Implementasi Modul Loading Screen
 *
 * Handle tampilan loading screen dan sequence loading asset game.
 * Asset dimuat sekali saja saat pertama kali Start Game, kemudian reuse.
 * Juga menangani transisi map dengan loading screen yang sama.
 */

#include "loading_screen.h"
#include "map.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "mainMenu.h"
#include "game_state_saver.h"
#include "screen.h"
#include "entities.h"
#include "propsbehavior.h"
#include "enemy_ai.h"
#include "seedmanager.h"
#include "worldgenio.h"
#include "fonts.h"
#include "../lib/raylib/include/raylib.h"

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
            state->loadingText = isBack ? "Returning to previous map..." : "Unloading current map...";
            UnloadMap();
            spawnFlowFields.clear();
            state->loadingStage++;
            state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
            break;

        case 1:
            state->loadingText = isBack ? "Reloading previous map..." : "Loading new map...";
            LoadMap(state->pendingMapPath.c_str());

            // Update map path segera agar IsAlreadyDead() pakai path yang benar
            SetCurrentMapPath(state->pendingMapPath.c_str());

            // Kalau ini worldgen stage, generate world pake seed yang sesuai
            if (!isBack && state->pendingMapPath.find("worldseed/save_") != std::string::npos)
            {
                int stageIdx = g_SeedManager.GetCurrentStage();
                uint64_t seed = g_SeedManager.GetSeed(stageIdx);
                RunWorldgen(seed, stageIdx == SeedManager::SEED_COUNT - 1);
                WorldgenIO::LoadRuntimeState(g_SeedManager.GetCurrentStage());
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
            state->loadingText = "Initializing player and entities...";
            // Re-init player berdasarkan target door di map baru
            PlayerInstance.Init(gState, state->pendingDoorName.c_str());

            // Bersihkan entitas map sebelumnya dan spawn entitas baru
            Entities::Clear();
            Entities::Add(&PlayerInstance);
            SpawnEnemiesFromMap();

            // Load musuh yang sudah ada atau spawn baru
            if (!LoadEnemiesForMap(state->pendingMapPath))
            {
                SpawnEnemiesFromMap();
            }

            // Load items
            if (!itemData.LoadItemsForMap(state->pendingMapPath))
            {
                SpawnItemWave();
            }

            state->loadingStage++;
            state->loadingProgress = (float)state->loadingStage / MAP_SWITCH_STAGES * 100.0F;
            break;

        case 3:
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
        // Kalo bukan map switch, reload map default (tutorial) agar player spawn fresh
        if (!state->isSwitchingMap && !state->isGoingBack)
        {
            InitMap();
            ClearSavedState(); // Fresh start — bersihkan state lama biar gak di-restore
        }

        state->loadingStage = TOTAL_LOADING_STAGES;
        state->loadingProgress = 100.0F;
        state->loadingComplete = true;
        state->currentScreen = PLAY;

        // Init first, then restore saved state - order matters!
        // InitAll() sets position to spawn, then RestoreGameState overwrites it
        InitAll();
        InitFonts();
        if (HasSavedState())
        {
            RestoreGameState(state);
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

        // Initialize everything first, then restore saved state
        InitAll();
        if (HasSavedState())
        {
            RestoreGameState(state);
        }
        InitMainMenu(state);
        break;
    }
}

/**
 * @brief RenderLoadingScreen()
 * Render loading screen ke layar virtual.
 * @param state Pointer ke GameState
 */
void RenderLoadingScreen(GameState *state)
{
    BeginTextureMode(state->Dungeon);
    ClearBackground(DARKGRAY);

    int textWidth = MeasureText(state->loadingText, 20);
    DrawText(state->loadingText, (GameScreenWidth / 2) - (textWidth / 2), (GameScreenHeight / 2) - 20, 20, WHITE);

    DrawRectangle((GameScreenWidth / 2) - 150, (GameScreenHeight / 2) + 20, 300, 20, DARKGRAY);
    DrawRectangle((GameScreenWidth / 2) - 150, (GameScreenHeight / 2) + 20, (int)(state->loadingProgress * 3), 20, GREEN);

    std::array<char, 10> progressText;
    sprintf(progressText.data(), "%d%%", (int)state->loadingProgress);
    int progressTextWidth = MeasureText(progressText.data(), 20);
    DrawText(progressText.data(), (GameScreenWidth / 2) - (progressTextWidth / 2), (GameScreenHeight / 2) + 50, 20, WHITE);

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
