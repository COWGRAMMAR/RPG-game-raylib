/**
 * @file main.cpp
 * @brief Entry Point Game Application
 *
 * Main game loop dan inisialisasi semua sistem.
 * Handle state management (MAIN_MENU, PLAY, OPTIONS) dan pause menu.
 */

#include "screen.h"
#include "map.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "mainMenu.h"
#include "pauseMenu.h"
#include "loading_screen.h"
#include "worldgenio.h"
#include "seedmanager.h"
#include "fonts.h"
#include "../lib/raylib/include/raylib.h"
#include "input.h"
#include "keybindManager.h"
#include "../lib/raylib/include/raymath.h"
#include <cstdio>

/**
 * @brief Global instances for menu systems
 */
PauseMenu pauseMenu;
OptionsScreen optionsScreen;

/**
 * @brief Main entry point game application
 * @return 0 saat game ditutup
 */
int main()
{
    // Inisialisasi
    // Urutan penting: InitScreen dulu → (loading screen will handle InitMap, InitAll, InitMainMenu)
    // InitMap harus sebelum InitAll karena player butuh data map buat spawn

    // Step 1: buat window, audio, dan render texture virtual (1280x720)
    GameState state = InitScreen();
    state.previousScreen = MAIN_MENU; // Default return to main menu
    gState = &state;

    // Initialize loading state variables
    state.loadingProgress = 0.0f;
    state.loadingText = "";
    state.loadingComplete = false;
    state.loadingStage = 0;
    state.assetsLoaded = false;

    // Step 5: init options screen (hidden initially)
    optionsScreen.Show();
    optionsScreen.Hide();

    // Step 6: init main menu (needed for menu buttons to render)
    InitMainMenu(&state);

    InitFonts();

    // Load keybinds (or save defaults on first run)
    if (!keybindManager.LoadFromFile("saves/settings.json"))
        keybindManager.SaveToFile("saves/settings.json");

    float accumulator = 0.0f;

    // Main Game Loop
    while (!WindowShouldClose())
    {
        // State: MAIN_MENU
        if (state.currentScreen == MAIN_MENU)
        {
            UpdateGame(&state);
            UpdateMainMenu(&state);
            RenderMainMenuToVirtualScreen(&state);
            DrawRenderWindows(&state);
        }
        /*==============================================================================
         * State: LOADING
         *==============================================================================*/
        else if (state.currentScreen == LOADING)
        {
            // Initialize loading screen on first entry or after returning from MAIN_MENU
            if (!state.enteredLoading || state.loadingComplete)
            {
                state.enteredLoading = false;
                state.loadingComplete = false;
                InitLoadingScreen(&state);
            }

            // Update and render loading screen
            UpdateLoadingScreen(&state);
            RenderLoadingScreen(&state);
            DrawRenderWindows(&state);
        }
        // State: OPTIONS
        else if (state.currentScreen == OPTIONS)
        {
            // Show options screen and set return screen on first entry
            if (!optionsScreen.IsActive())
            {
                optionsScreen.SetReturnScreen(state.previousScreen);
                optionsScreen.Show();
            }

            UpdateGame(&state);
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            optionsScreen.Update(&state, GetVirtualMousePosition(&state), mouseClicked);

            BeginTextureMode(state.Dungeon);
            ClearBackground(DARKGRAY);
            optionsScreen.Draw(GetVirtualMousePosition(&state));
            EndTextureMode();

            DrawRenderWindows(&state);
        }
        // State: PLAY
        else if (state.currentScreen == PLAY)
        {
            // If returning from OPTIONS, ensure pause menu is shown
            if (state.previousScreen == OPTIONS && !pauseMenu.IsActive())
            {
                pauseMenu.Show();
            }

            // Poll input FIRST so pause toggle uses fresh state
            InputInstance.PollInput();

            if (InputInstance.GetState().pauseMenu)
            {
                if (pauseMenu.IsActive())
                    pauseMenu.Hide();
                else
                    pauseMenu.Show();
            }

            // update scale sebelum rendering
            UpdateGame(&state);

            // capture mouse click before rendering
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

            // update pause menu if active (HARUS sebelum rendering)
            if (pauseMenu.IsActive())
            {
                pauseMenu.Update(&state, GetVirtualMousePosition(&state), mouseClicked);
            }

            // Fixed timestep
            float frameTime = GetFrameTime();

            if (frameTime > Time::MAX_FRAME)
                frameTime = Time::MAX_FRAME;

            accumulator += frameTime;

            // update semua logic game - skip when paused
            while (accumulator >= Time::DELTA_TIME)
            {
                if (!pauseMenu.IsActive())
                {
                    UpdateLogicAll();
                }

                accumulator -= Time::DELTA_TIME;
            }

            // render semua ke layar virtual
            DrawRenderTexture(&state);

            // scale layar virtual ke window asli
            DrawRenderWindows(&state);
        }
    }

    // Auto-save sebelum exit (jika run masih aktif)
    if (g_SeedManager.IsRunActive())
        WorldgenIO::SaveRuntimeState(g_SeedManager.GetCurrentStage());

    // Shutdown
    GameShutDown(&state);
    return 0;
}
