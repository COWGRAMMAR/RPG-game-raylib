/**
 * @file main.cpp
 * @brief Entry Point Game Application
 *
 * Main game loop dan inisialisasi semua sistem.
 * Handle state management (MAIN_MENU, PLAY, OPTIONS) dan pause menu.
 */

#include "../../include/core/screen.h"
#include "../../include/map/map.h"
#include "../../include/entities/player.h"
#include "../../include/entities/enemy.h"
#include "../../include/items/item.h"
#include "../../include/ui/mainMenu.h"
#include "../../include/ui/pauseMenu.h"
#include "../../include/ui/gameOverScreen.h"
#include "../../include/ui/saveLoadScreen.h"
#include "../../include/core/loading_screen.h"
#include "../../include/core/game_state_saver.h"
#include "../../include/core/savemanager.h"
#include "../../include/map/worldgenio.h"
#include "../../include/core/seedmanager.h"
#include "../../include/rendering/fonts.h"
#include "../../include/systems/input.h"
#include "../../include/systems/keybindManager.h"
#include "../../include/ui/videoScreen.h"
#include "../../include/ui/videoTab.h"
#include "../../include/ui/audioTab.h"
#include "../../include/systems/audioManager.h"
#include "../../include/rendering/hud.h"
#include "../../include/map/propsbehavior.h"
#include "raylib.h"
#include "raymath.h"
#include <cstdio>
#include <filesystem>

/**
 * @brief Global instances for menu systems
 */
PauseMenu pauseMenu;
OptionsScreen optionsScreen;
SaveLoadScreen saveLoadScreen;
VideoScreen videoScreen;

/**
 * @brief Custom TraceLog callback to prepend HH:mm:ss.fff timestamps
 */
static void TimestampLog(int msgType, const char *text, va_list args)
{
    double now = GetTime();
    int hours = (int)(now / 3600) % 24;
    int minutes = (int)(now / 60) % 60;
    int seconds = (int)now % 60;
    int millis = (int)((now - (int)now) * 1000);

    char buf[1024] = {0};
    vsnprintf(buf, sizeof(buf), text, args);

    const char *level = "";
    switch (msgType)
    {
    case LOG_TRACE:
        level = "TRACE";
        break;
    case LOG_DEBUG:
        level = "DEBUG";
        break;
    case LOG_INFO:
        level = "INFO";
        break;
    case LOG_WARNING:
        level = "WARNING";
        break;
    case LOG_ERROR:
        level = "ERROR";
        break;
    case LOG_FATAL:
        level = "FATAL";
        break;
    default:
        level = "UNKNOWN";
        break;
    }

    printf("%02d:%02d:%02d.%03d %s: %s\n", hours, minutes, seconds, millis, level, buf);
}

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

    // Override: mulai dari VIDEO (intro) sebelum MAIN_MENU
    state.currentScreen = VIDEO;

    // Register custom TraceLog callback for timestamps
    SetTraceLogCallback(TimestampLog);

    // Initialize loading state variables
    state.loadingProgress = 0.0f;
    state.loadingText = "";
    state.loadingComplete = false;
    state.loadingStage = 0;
    state.assetsLoaded = false;
    state.enteredLoading = false;

    // Muat pengaturan video SEBELUM init options screen
    LoadVideoSettings(&state);

    // Step 5: init options screen (hidden initially) — sync dari state yang udah di-load
    optionsScreen.Show(&state);
    optionsScreen.Hide();

    // Step 6: init main menu (needed for menu buttons to render)
    InitMainMenu(&state);

    // Migrasi satu kali: saves/settings.json -> saves/settings/keybindsTab.json
    {
        namespace fs = std::filesystem;
        fs::create_directories("saves/settings");
        const char *target = "saves/settings/keybindsTab.json";
        if (fs::exists("saves/settings.json"))
        {
            fs::rename("saves/settings.json", target);
            TraceLog(LOG_INFO, "KEYBIND: settings.json dimigrasi ke keybindsTab.json");
        }
        else if (fs::exists("saves/settings/keybinds.json"))
        {
            fs::rename("saves/settings/keybinds.json", target);
            TraceLog(LOG_INFO, "KEYBIND: keybinds.json dimigrasi ke keybindsTab.json");
        }
    }

    // Load keybinds (or save defaults on first run)
    if (!keybindManager.LoadFromFile("saves/settings/keybindsTab.json"))
        keybindManager.SaveToFile("saves/settings/keybindsTab.json");

    // Inisialisasi AudioManager dan muat aset audio
    AudioManager::Init();
    AudioManager::LoadAudioAssets();

    // Muat pengaturan audio
    LoadAudioSettings();

    float accumulator = 0.0f;

    // Main Game Loop
    while (!WindowShouldClose())
    {
        // Update audio system setiap frame (UpdateMusicStream + auto-switch track)
        AudioManager::Update(state.currentScreen);
        // Boss music ambient — deteksi proximity, play/stop boss track
        UpdateBossMusic();

        // Toggle fullscreen global dari screen mana pun
        if (IsKeyPressed(keybindManager.GetKeycode(TOGGLE_FULLSCREEN)))
        {
            ToggleFullscreenMode();
            GScreenWidth = GetScreenWidth();
            GScreenHeight = GetScreenHeight();
        }

        // ===== State: VIDEO (intro saat startup) =====
        if (state.currentScreen == VIDEO)
        {
            // Muat dan putar video hanya sekali saat pertama masuk
            static bool videoStarted = false;
            if (!videoStarted)
            {
                videoScreen.LoadAndPlay();
                videoStarted = true;
            }

            // Delta time, cap maks 1/20 detik biar ga loncat jauh
            float deltaTime = GetFrameTime();
            if (deltaTime > 0.05f)
                deltaTime = 0.05f;

            // Update video; jika selesai/skip -> pindah ke MAIN_MENU
            if (videoScreen.Update(deltaTime))
            {
                videoScreen.Unload();
                state.currentScreen = MAIN_MENU;
                videoStarted = false;
            }

            if (WindowShouldClose())
                break;

            // Render video fullscreen (tanpa virtual screen biar ga distretch)
            BeginDrawing();
            ClearBackground(BLACK);
            videoScreen.Draw();
            EndDrawing();
        }
        // ===== State: MAIN_MENU =====
        else if (state.currentScreen == MAIN_MENU)
        {
            UpdateGame(&state);
            UpdateMainMenu(&state);
            if (WindowShouldClose())
                break;
            RenderMainMenuToVirtualScreen(&state);
            DrawRenderWindows(&state);
        }
        // ===== State: LOADING =====
        else if (state.currentScreen == LOADING)
        {
            // Initialize loading screen on first entry or after returning from MAIN_MENU
            if (!state.enteredLoading || state.loadingComplete)
            {
                state.enteredLoading = false;
                state.loadingComplete = false;
                InitLoadingScreen(&state);
            }
            UpdateLoadingScreen(&state);
            if (WindowShouldClose())
                break;
            RenderLoadingScreen(&state);
            DrawRenderWindows(&state);
        }
        // ===== State: OPTIONS =====
        else if (state.currentScreen == OPTIONS)
        {
            if (!optionsScreen.IsActive())
            {
                optionsScreen.SetReturnScreen(state.previousScreen);
                optionsScreen.Show(&state);
            }
            UpdateGame(&state);
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            optionsScreen.Update(&state, GetVirtualMousePosition(&state), mouseClicked);
            if (WindowShouldClose())
                break;
            BeginTextureMode(state.Dungeon);
            DrawMenuBackground();
            optionsScreen.Draw(GetVirtualMousePosition(&state));
            EndTextureMode();
            DrawRenderWindows(&state);
        }
        // ===== State: PLAY =====
        else if (state.currentScreen == PLAY)
        {
            UpdateGame(&state);

            gState = &state;

            // If returning from OPTIONS, ensure pause menu is shown
            if (state.previousScreen == OPTIONS && !pauseMenu.IsActive())
                pauseMenu.Show();

            // Poll input FIRST so pause toggle uses fresh state
            InputInstance.PollInput();

            if (InputInstance.GetState().pauseMenu)
            {
                if (pauseMenu.IsActive())
                    pauseMenu.Hide();
                else
                    pauseMenu.Show();
            }

            // capture mouse click before rendering
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

            // update pause menu if active (HARUS sebelum rendering)
            if (pauseMenu.IsActive())
                pauseMenu.Update(&state, GetVirtualMousePosition(&state), mouseClicked);

            // Flush input bila pause menu mengubah layar (misal "To Main" → MAIN_MENU)
            if (state.currentScreen != PLAY)
                PollInputEvents();

            // Fixed timestep
            float frameTime = GetFrameTime();
            if (frameTime > Time::MAX_FRAME)
                frameTime = Time::MAX_FRAME;
            // Autosave timer - only count when not paused
            static float autosaveTimer = 0.0f;
            if (!pauseMenu.IsActive())
            {
                autosaveTimer += frameTime;
                if (autosaveTimer >= 60.0f)
                {
                    SaveManager::SaveAutosave(g_ActiveSaveSlot);
                    autosaveTimer = 0.0f;
                }
            }

            accumulator += frameTime;

            // update semua logic game - skip when paused, dialog active, or screen changed
            while (accumulator >= Time::DELTA_TIME)
            {
                if (!pauseMenu.IsActive() && !signManager.IsDialogActive() && state.currentScreen == PLAY)
                {
                    UpdateLogicAll();
                    if (PlayerInstance.Anim.isDead)
                    {
                        state.currentScreen = GAME_OVER;
                    }
                }
                accumulator -= Time::DELTA_TIME;
            }

            // dismiss dialog via left-click — skip kalo pause aktif
            if (signManager.IsDialogActive() && !pauseMenu.IsActive() && InputInstance.IsLeftClickPressed())
            {
                signManager.DismissDialog();
            }

            // render hanya jika masih dalam PLAY state
            if (state.currentScreen == PLAY)
            {
                DrawRenderTexture(&state);
                DrawRenderWindows(&state);
            }
        }
        // ===== State: GAME_OVER =====
        else if (state.currentScreen == GAME_OVER)
        {
            UpdateGameOverScreen(&state);
            if (WindowShouldClose())
                break;
            RenderGameOverScreen(&state);
            DrawRenderWindows(&state);
        }
        // ===== State: SAVE_LOAD =====
        else if (state.currentScreen == SAVE_LOAD)
        {
            if (!saveLoadScreen.IsActive())
            {
                saveLoadScreen.SetReturnScreen(state.previousScreen);
                saveLoadScreen.Show();
            }
            UpdateGame(&state);
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            saveLoadScreen.Update(&state, GetVirtualMousePosition(&state), mouseClicked);
            if (WindowShouldClose())
                break;
            BeginTextureMode(state.Dungeon);
            DrawMenuBackground();
            saveLoadScreen.Draw(GetVirtualMousePosition(&state));
            EndTextureMode();
            DrawRenderWindows(&state);
        }
    }

    if (g_SeedManager.IsRunActive())
    {
        GameSnapshot snap = SaveManager::CaptureSnapshot();
        SaveManager::SaveManual(snap, g_ActiveSaveSlot);
    }

    // Shutdown audio sebelum close audio device
    AudioManager::Shutdown();

    // Shutdown
    GameShutDown(&state);
    return 0;
}
