/**
 * @file main.cpp
 * @brief Entry Point Game Application
 *
 * Main game loop dan inisialisasi semua sistem.
 * Handle state management (MAIN_MENU, PLAY, OPTIONS) dan pause menu.
 */

#include "core/screen.h"
#include "map/map.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "items/item.h"
#include "ui/mainMenu.h"
#include "ui/pauseMenu.h"
#include "ui/gameOverScreen.h"
#include "ui/saveLoadScreen.h"
#include "core/loading_screen.h"
#include "core/game_state_saver.h"
#include "core/savemanager.h"
#include "map/worldgenio.h"
#include "core/seedmanager.h"
#include "rendering/fonts.h"
#include "systems/input.h"
#include "systems/keybindManager.h"
#include "ui/videoScreen.h"
#include "ui/videoTab.h"
#include "ui/audioTab.h"
#include "systems/audioManager.h"
#include "rendering/hud.h"
#include "map/propsbehavior.h"
#include "map/minimap.h"
#include "raylib.h"
#include "raymath.h"
#include <cstdio>
#include <filesystem>

/**
 * @brief Batas atas delta time per frame untuk mencegah lompatan besar
 */
static const float MAX_DELTA_TIME = 0.05f;

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

    while (!WindowShouldClose())
    {
        AudioManager::Update(state.currentScreen);
        // Boss music ambient — deteksi proximity, play/stop boss track
        UpdateBossMusic();

        if (IsKeyPressed(keybindManager.GetKeycode(TOGGLE_FULLSCREEN)))
        {
            ToggleFullscreenMode();
        }

        // State: VIDEO (intro saat startup)
        if (state.currentScreen == VIDEO)
        {
            enum class VPhase : uint8_t { INTRODUCTION, DONE };
            static VPhase videoPhase = VPhase::INTRODUCTION;
            static bool videoStarted = false;

            if (!videoStarted)
            {
                if (videoPhase == VPhase::INTRODUCTION)
                {
                    videoScreen.SetVideoPath("assets/video/intro/IntroIntroductions.mkv");
                    AudioManager::PlayTrack("MainMenu");
                }
                videoScreen.SetVolume(AudioManager::GetVideoVolume());
                videoScreen.LoadAndPlay();
                videoStarted = true;
            }

            float deltaTime = GetFrameTime();
            if (deltaTime > MAX_DELTA_TIME)
                deltaTime = MAX_DELTA_TIME;

            videoScreen.SetVolume(AudioManager::GetVideoVolume());

            if (videoScreen.Update(deltaTime))
            {
                videoScreen.Unload();
                videoPhase = VPhase::DONE;
                state.currentScreen = MAIN_MENU;
                videoStarted = false;
            }

            if (WindowShouldClose())
                break;

            BeginDrawing();
            ClearBackground(BLACK);
            videoScreen.Draw();
            EndDrawing();
        }
        else if (state.currentScreen == MAIN_MENU)
        {
            UpdateGame(&state);
            UpdateMainMenu(&state);
            if (WindowShouldClose())
                break;
            RenderMainMenuToVirtualScreen(&state);
            DrawRenderWindows(&state);
        }
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
                    SaveManager::SaveAutosave(-1);
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

            // Update minimap setelah logic player — biar UpdateView pake posisi player terbaru
            MinimapSystem::Update();

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

    // Shutdown sistem tersier (sebelum GameShutDown)
    MinimapSystem::Shutdown();

    // Shutdown
    GameShutDown(&state);
    return 0;
}
