/**
 * @file pauseMenu.cpp
 * @brief Implementasi dari Pause Menu System dan Options Screen
 *
 * Handle pause menu UI dan standalone options screen dengan tabs.
 */

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "systems/keybindManager.h"
#include "rendering/fonts.h"
#include "rendering/hud.h"
#include "ui/pauseMenu.h"
#include "ui/popup.h"
#include "ui/videoTab.h"
#include "ui/audioTab.h"
#include "ui/keybindsTab.h"
#include "ui/saveLoadScreen.h"
#include "ui/mainMenu.h"
#include "core/game_state_saver.h"
#include "map/minimap.h"
#include "systems/audioManager.h"
#include "core/savemanager.h"
#include "map/worldgenio.h"
#include "core/seedmanager.h"
#include "entities.h"
#include "item.h"
#include "propsbehavior.h"
#include "enemy.h"
#include "enemy_ai.h"
#include "map.h"
#include "mapLogic.h"
#include "systems/combatTurn.h"

/*==============================================================================
 * External References
 *==============================================================================*/

extern SaveLoadScreen saveLoadScreen;

/*==============================================================================
 * Static Variables (Popup Notifications)
 *==============================================================================*/

static Popup savePopup("Game Saved!", "OK", 0.7F);
static Popup saveErrorPopup("Failed to save game.", "OK", 0.7F);
static Popup loadConfirmPopup("Load from save? Current progress will be lost.", "Load Save", "Cancel", 0.7f);
static Popup noSavePopup("No save file found.", "OK", 0.7F);
static Popup pauseCorruptPopup("Save data is corrupted or incompatible.", "Yes, Delete", "No", 0.7f);
static Popup returnConfirmPopup("Return to main menu?", "Continue", "Cancel", 0.7f);
static Popup restartConfirmPopup("Restart run?", "Restart", "Cancel", 0.7f);

/*==============================================================================
 * OptionsScreen Implementation
 *==============================================================================*/

/**
 * @brief Constructor
 *
 * Menginisialisasi semua tombol tab, tombol back, dan dimensi awal.
 */
OptionsScreen::OptionsScreen() : active(false), texturesLoaded(false), returnScreen(PLAY), selectedTab(0), showFPS(false), width(0), height(0), startX(0), startY(0), bgTexture({0})
{
}

/**
 * @brief Destructor
 */
OptionsScreen::~OptionsScreen()
{
    if (bgTexture.id != 0)
    {
        UnloadTexture(bgTexture);
    }
}

/**
 * @brief Menampilkan layar options
 */
void OptionsScreen::Show(GameState *state)
{
    active = true;
    if (!texturesLoaded)
    {
        Image img = LoadImage("assets/textures/settingsButt/settingsBG.png");
        if (img.data != nullptr)
        {
            bgTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
        texturesLoaded = true;
    }
    CalculateDimensions();
    showFPS = state->showFPS;
}

/**
 * @brief Menyembunyikan layar options
 */
void OptionsScreen::Hide()
{
    active = false;
    UnloadKeybindsTextures();
    UnloadAudioTextures();
}

/**
 * @brief Memeriksa apakah layar options sedang aktif
 * @return true jika aktif, false jika tidak
 */
bool OptionsScreen::IsActive() const
{
    return active;
}

/**
 * @brief Mendapatkan daftar resolusi yang tersedia berdasarkan monitor
 * @return Vektor berisi ResOption (width, height, label)
 */
std::vector<ResOption> GetAvailableResolutions()
{
    std::vector<ResOption> options;

    Rectangle monitor = GetMonitorResolution();
    int maxWidth = static_cast<int>(monitor.width);
    int maxHeight = static_cast<int>(monitor.height);

    if (1280 <= maxWidth && 720 <= maxHeight)
    {
        options.push_back({1280, 720, "720p"});
    }
    if (1920 <= maxWidth && 1080 <= maxHeight)
    {
        options.push_back({1920, 1080, "1080p"});
    }
    if (2560 <= maxWidth && 1440 <= maxHeight)
    {
        options.push_back({2560, 1440, "1440p"});
    }
    if (3840 <= maxWidth && 2160 <= maxHeight)
    {
        options.push_back({3840, 2160, "4K"});
    }

    if (options.empty())
    {
        options.push_back({1280, 720, "720p"});
    }

    return options;
}

/**
 * @brief Menghitung dimensi dan membuat elemen UI
 *
 * Menggunakan Approach B: selalu mulai dari opsi pertama (720p)
 * tanpa melakukan auto-detect resolusi saat ini.
 */
void OptionsScreen::CalculateDimensions()
{
    const int tabHeight = 56;

    width = bgTexture.width > 0 ? bgTexture.width : 800;
    height = bgTexture.height > 0 ? bgTexture.height : 600;
    startX = (GameScreenWidth - width) / 2;
    startY = (GameScreenHeight - height) / 2;

    backgroundRect = {static_cast<float>(startX), static_cast<float>(startY), static_cast<float>(width), static_cast<float>(height)};

    int tabY = startY + 15 + 120;

    const char *tabFiles[3] = {
        "assets/textures/settingsButt/settingsVideo.png",
        "assets/textures/settingsButt/settingsAudio.png",
        "assets/textures/settingsButt/settingsKeybinds.png"};
    for (int i = 0; i < 3; i++)
    {
        float cx = static_cast<float>(startX + width / 2 + (i - 1) * 302);
        float cy = static_cast<float>(tabY + tabHeight / 2);
        tabButtons[i] = buttonImage(tabFiles[i], Vector2{cx, cy}, 1.0F, 0.7F);
    }

    backButton = buttonImage(
        "assets/textures/settingsButt/settingsBack.png",
        Vector2{static_cast<float>(startX + width - 133),
                static_cast<float>(startY + height - 53)},
        1.0F, 0.7F);

    if (resolutionOptions.empty())
    {
        resolutionOptions = GetAvailableResolutions();
    }

    const int labelFontSize = 24;
    int valueX = startX + 339;
    int contentStartY = startY + 221;

    bool isFullscreen = IsWindowFullscreen();
    fullscreenButton = buttonTxt(
        isFullscreen ? "ON" : "OFF",
        valueX,
        contentStartY + 15,
        labelFontSize,
        isFullscreen ? GREEN : RED,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));

    fpsButton = buttonTxt(
        showFPS ? "ON" : "OFF",
        valueX,
        contentStartY + 75,
        labelFontSize,
        showFPS ? GREEN : GRAY,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));

    resetTabButton = buttonImage( // basch-3: diganti dari buttonTxt ke buttonImage
        "assets/textures/settingsButt/resetTab.png",
        Vector2{static_cast<float>(startX + 121),
                static_cast<float>(startY + height - 54)},
        1.0F, 0.7F);

    resetOptionsButton = buttonImage( // basch-3: diganti dari buttonTxt ke buttonImage
        "assets/textures/settingsButt/resetAll.png",
        Vector2{static_cast<float>(startX + 282),
                static_cast<float>(startY + height - 54)},
        1.0F, 0.7F);
}

/**
 * @brief Memperbarui handling input
 * @param state Pointer ke GameState
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 */
void OptionsScreen::Update(GameState *state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active)
    {
        return;
    }

    if (backButton.isClicked(mousePosition, mouseClicked))
    {
        active = false;
        state->currentScreen = returnScreen;
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        if (tabButtons[i].isClicked(mousePosition, mouseClicked))
        {
            selectedTab = i;
            CalculateDimensions();
            return;
        }
    }

    if (resetTabButton.isClicked(mousePosition, mouseClicked))
    {
        const char *paths[] = {
            "saves/settings/videoTab.json",
            "saves/settings/audioTab.json",
            "saves/settings/keybindsTab.json"};
        std::error_code ec;
        std::filesystem::remove(paths[selectedTab], ec);

        if (selectedTab == 0)
        {
            if (IsWindowFullscreen())
                ToggleFullscreenMode();
            state->showFPS = false;
            showFPS = false;
        }
        else if (selectedTab == 1)
        {
            g_sliders = {100, 100, 100, 100, false, -1};
            AudioManager::SetVolumesFromPct(100, 100, 100, 100);
            SaveAudioSettings(g_sliders.masterVolume, g_sliders.musicVolume, g_sliders.sfxVolume, g_sliders.videoVolume);
        }
        else if (selectedTab == 2)
        {
            keybindManager.ResetDefaults();
            keybindManager.SaveToFile(paths[2]);
        }
        CalculateDimensions();
        return;
    }

    if (resetOptionsButton.isClicked(mousePosition, mouseClicked))
    {
        const char *allPaths[] = {
            "saves/settings/videoTab.json",
            "saves/settings/audioTab.json",
            "saves/settings/keybindsTab.json"};
        std::error_code ec;
        for (const auto &p : allPaths)
        {
            std::filesystem::remove(p, ec);
        }

        if (IsWindowFullscreen())
            ToggleFullscreenMode();
        state->showFPS = false;
        showFPS = false;
        g_sliders = {100, 100, 100, 100, false, -1};
        AudioManager::SetVolumesFromPct(100, 100, 100, 100);
        SaveAudioSettings(g_sliders.masterVolume, g_sliders.musicVolume, g_sliders.sfxVolume, g_sliders.videoVolume);
        keybindManager.ResetDefaults();
        keybindManager.SaveToFile("saves/settings/keybindsTab.json");
        CalculateDimensions();
        return;
    }

    if (selectedTab == 0)
    {
        if (UpdateVideoTab(fullscreenButton, fpsButton, state, mousePosition, mouseClicked))
        {
            showFPS = state->showFPS;
            CalculateDimensions();
        }
    }

    if (selectedTab == 1)
    {
        UpdateAudioTab(g_sliders, mousePosition, mouseClicked, startX + 89, startY + 121);
    }
}

/**
 * @brief Me-render layar options
 * @param mousePosition Posisi mouse untuk efek hover
 */
void OptionsScreen::Draw(Vector2 mousePosition)
{
    if (!active)
    {
        return;
    }

    if (bgTexture.id != 0)
    {
        DrawTexture(bgTexture, startX, startY, WHITE);
    }
    else
    {
        Color bgColor = {40, 40, 40, 230};
        DrawRectangleRec(backgroundRect, bgColor);
        DrawRectangleLinesEx(backgroundRect, 2, WHITE);
    }

    for (int i = 0; i < 3; i++)
    {
        tabButtons[i].Draw(mousePosition);
    }

    backButton.Draw(mousePosition);

    int contentOX = 89, contentOY = 121;
    Rectangle contentRect = {
        static_cast<float>(startX + contentOX + 20),
        static_cast<float>(startY + 200),
        static_cast<float>(width - 2 * (contentOX + 20)),
        static_cast<float>(height - 323)};
    DrawRectangleRec(contentRect, {0, 0, 0, 51});

    switch (selectedTab)
    {
    case 0:
        DrawVideoTab(fullscreenButton, fpsButton, mousePosition, startX + contentOX, startY + contentOY);
        break;
    case 1:
        DrawAudioTab(mousePosition, startX + contentOX, startY + contentOY);
        break;
    case 2:
        DrawKeybindsTab(mousePosition, startX + contentOX, startY + contentOY, (int)contentRect.width);
        break;
    }

    // basch-3: background hitam di belakang tombol dihapus (tidak perlu untuk buttonImage)
    resetTabButton.Draw(mousePosition);
    resetOptionsButton.Draw(mousePosition);
}

/*==============================================================================
 * PauseMenu Implementation
 *==============================================================================*/

static const char *BUTTON_PATHS[7] = {
    "assets/textures/pauseButt/pause-resume.png",
    "assets/textures/pauseButt/pause-save.png",
    "assets/textures/pauseButt/pause-load.png",
    "assets/textures/pauseButt/pause-settings.png",
    "assets/textures/pauseButt/pause-restart.png",
    "assets/textures/pauseButt/pause-tomain.png",
    "assets/textures/pauseButt/pause-exit.png"};

/**
 * @brief Constructor
 */
PauseMenu::PauseMenu()
    : active(false), texturesLoaded(false), bgTexture({0}), position({0, 0}), width(0), height(0)
{
}

/**
 * @brief Destructor
 */
PauseMenu::~PauseMenu()
{
    if (bgTexture.id != 0)
        UnloadTexture(bgTexture);
}

/**
 * @brief Memuat texture dari disk (lazy, sekali saja)
 */
void PauseMenu::LoadTextures()
{
    if (texturesLoaded)
        return;
    texturesLoaded = true;

    // set background texture untuk semua popup notifikasi pause
    savePopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    saveErrorPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    loadConfirmPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    noSavePopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    pauseCorruptPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    returnConfirmPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    restartConfirmPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    restartConfirmPopup.SetTextYOffset(15);
    restartConfirmPopup.SetButtonYOffset(-20);
    restartConfirmPopup.SetTextColor(BLACK); // basch-3: teks hitam
    // offset khusus returnConfirmPopup agar teks & tombol sejajar
    returnConfirmPopup.SetTextYOffset(15);
    returnConfirmPopup.SetButtonYOffset(-15);
    returnConfirmPopup.SetTextColor(BLACK); // basch-3: teks hitam

    Image img = LoadImage("assets/textures/pauseButt/pause-bg.png");
    bgTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    width = bgTexture.width;
    height = bgTexture.height;
    position.x = (GameScreenWidth - width) / 2.0F;
    position.y = (GameScreenHeight - height) / 2.0F;
    backgroundRect = {position.x, position.y, static_cast<float>(width), static_cast<float>(height)};

    float centerX = position.x + width / 2.0F;
    // jarak vertikal antar baris tombol diperlebar dari 17 → 28
    float gap = 28.0F;
    float btnHeight = 56.0F;
    float pairGap = 105.0F;
    float totalBtnHeight = 5.0F * btnHeight + 4.0F * gap;
    // startY ditambah 40px agar tombol pause lebih ke bawah (25 → 65)
    float startY = position.y + (height - totalBtnHeight) / 2.0F + 35.0F;

    // Row 0: Resume (wide, centered)
    {
        float btnY = startY + btnHeight / 2.0F;
        buttons[0] = buttonImage(BUTTON_PATHS[0], Vector2{centerX, btnY}, 1.0F, 0.6F);
    }

    // Row 1: Save (left) / Load (right)
    {
        float btnY = startY + 1.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[1] = buttonImage(BUTTON_PATHS[1], Vector2{centerX - pairGap, btnY}, 1.0F, 0.6F);
        buttons[2] = buttonImage(BUTTON_PATHS[2], Vector2{centerX + pairGap, btnY}, 1.0F, 0.6F);
    }

    // Row 2: Settings (wide, centered)
    {
        float btnY = startY + 2.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[3] = buttonImage(BUTTON_PATHS[3], Vector2{centerX, btnY}, 1.0F, 0.6F);
    }

    // Row 3: Restart (left) / To Main (right)
    {
        float btnY = startY + 3.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[4] = buttonImage(BUTTON_PATHS[4], Vector2{centerX - pairGap, btnY}, 1.0F, 0.6F);
        buttons[5] = buttonImage(BUTTON_PATHS[5], Vector2{centerX + pairGap, btnY}, 1.0F, 0.6F);
    }

    // Row 4: Exit (wide, centered)
    {
        float btnY = startY + 4.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[6] = buttonImage(BUTTON_PATHS[6], Vector2{centerX, btnY}, 1.0F, 0.6F);
    }
}

/**
 * @brief Menampilkan pause menu
 */
void PauseMenu::Show()
{
    LoadTextures();
    active = true;
}

/**
 * @brief Menyembunyikan pause menu
 */
void PauseMenu::Hide()
{
    active = false;
}

/**
 * @brief Memeriksa apakah pause menu sedang aktif
 * @return true jika aktif, false jika tidak
 */
bool PauseMenu::IsActive() const
{
    return active;
}

/**
 * @brief Menghitung ulang dimensi (dummy — virtual screen fixed, tidak perlu)
 */
void PauseMenu::CalculateDimensions()
{
    // Virtual screen 1280x720 is fixed; textures loaded once in LoadTextures()
}

/**
 * @brief Handle klik pada tombol berdasarkan index
 * @param buttonIndex Index tombol yang diklik (0-5)
 * @param state Pointer ke GameState
 */
void PauseMenu::HandleButtonClick(int buttonIndex, GameState *state)
{
    switch (buttonIndex)
    {
    case 0: // Resume
        Hide();
        break;
    case 1:
        // Save Game — buka SaveLoadScreen dalam mode save
        state->previousScreen = PLAY;
        saveLoadScreen.SetMode(SaveLoadMode::SAVE_MODE);
        state->currentScreen = SAVE_LOAD;
        Hide();
        break;
    case 2:
        // Load Game — buka SaveLoadScreen dalam mode load
        state->previousScreen = PLAY;
        saveLoadScreen.SetMode(SaveLoadMode::LOAD_MODE);
        state->currentScreen = SAVE_LOAD;
        Hide();
        break;
    case 3: // Settings
        state->currentScreen = OPTIONS;
        state->previousScreen = PLAY;
        Hide();
        break;
    case 4: // Restart
        restartConfirmPopup.SetSubMessage("Current progress will be lost.");
        restartConfirmPopup.Show();
        break;
    case 5: // To Main Menu
        returnConfirmPopup.SetSubMessage("Unsaved progress will be lost.");
        returnConfirmPopup.Show();
        break;
    case 6: // Exit Game
        WorldgenIO::CleanupOrphanedSlots();
        CloseWindow();
        break;
    default:
        break;
    }
}

/**
 * @brief Memperbarui logic pause menu
 * @param state Pointer ke GameState
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 */
void PauseMenu::Update(GameState *state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active)
    {
        return;
    }

    if (savePopup.IsActive())
    {
        savePopup.Update(mousePosition, mouseClicked);
        return;
    }

    if (saveErrorPopup.IsActive())
    {
        saveErrorPopup.Update(mousePosition, mouseClicked);
        return;
    }

    if (loadConfirmPopup.IsActive())
    {
        loadConfirmPopup.Update(mousePosition, mouseClicked);
        if (loadConfirmPopup.IsConfirmClicked())
        {
            {
                bool snapshotValid = SaveManager::HasManual(g_ActiveSaveSlot);
                if (snapshotValid)
                {
                    GameSnapshot snap = SaveManager::LoadManual(g_ActiveSaveSlot);
                    snapshotValid = (snap.version == GameSnapshot::SNAPSHOT_VERSION);
                }

                if (snapshotValid)
                {
                    loadConfirmPopup.Hide();
                    state->enteredLoading = false;
                    state->loadingStage = 0;
                    state->loadingProgress = 0.0F;
                    state->loadingComplete = false;
                    state->currentScreen = LOADING;
                    Hide();
                }
                else
                {
                    loadConfirmPopup.Hide();
                    pauseCorruptPopup.SetSubMessage("This save data will be deleted.");
                    pauseCorruptPopup.Show();
                }
            }
        }
        return;
    }

    if (noSavePopup.IsActive())
    {
        noSavePopup.Update(mousePosition, mouseClicked);
        return;
    }

    if (pauseCorruptPopup.IsActive())
    {
        pauseCorruptPopup.Update(mousePosition, mouseClicked);
        if (pauseCorruptPopup.IsConfirmClicked())
            SaveManager::DeleteSlot(g_ActiveSaveSlot);
        return;
    }

    if (returnConfirmPopup.IsActive())
    {
        returnConfirmPopup.Update(mousePosition, mouseClicked);
        if (returnConfirmPopup.IsConfirmClicked())
        {
            InputInstance.ResetMenuFlags();
            UnloadHUDTextures();

            // ── Free gameplay resources sebelum kembali ke menu ──
            TurnCombat::Shutdown();
            Entities::Clear();
            Entities::ClearDeadEntities();
            UnloadMap();
            mapHistoryStack.Clear();
            MinimapSystem::Shutdown();
            g_Minimap.fogCache.clear();

            CloseTextures();
            state->assetsLoaded = false;
            state->enteredLoading = false;
            state->loadingStage = 0;
            state->loadingProgress = 0.0F;
            state->loadingComplete = false;
            WorldgenIO::CleanupOrphanedSlots();
            InitMainMenu(state);
            state->currentScreen = MAIN_MENU;
            AudioManager::PlayTrack("MainMenu-Loop");
            AudioManager::ResetToScreenTrack();
            Hide();
        }
        return;
    }

    if (restartConfirmPopup.IsActive())
    {
        restartConfirmPopup.Update(mousePosition, mouseClicked);
        if (restartConfirmPopup.IsConfirmClicked())
        {
            restartConfirmPopup.Hide();

            // ── Clear runtime state ──
            TurnCombat::Shutdown();
            Entities::Clear();
            itemData.activeItems.clear();
            ClearTileProps();
            Entities::ClearDeadEntities();
            chestManager.ResetConsumed();
            spikeManager.Clear();
            bombManager.ResetConsumed();
            crateManager.ResetConsumed();
            barrierManager.Clear();
            mapHistoryStack.Clear();

            // Bersihin checkpoint basi dari session sebelumnya
            SaveManager::ClearWorkspaceCheckpoints();

            // ── Load snapshot: pure runtime workspace (-1) ──
            GameSnapshot snap;
            bool hasSnap = false;
            if (SaveManager::HasInitial(-1))
            {
                snap = SaveManager::LoadInitial(-1);
                hasSnap = true;
            }

            // ── Apply unified pipeline ──
            // 1. ApplyPreSpawn: dead entities + consumed props (SEBELUM spawn)
            // 2. Spawn: enemies + items pake deterministic UUID (Phase 1)
            // 3. ApplyPostSpawn: FULL restore — player, enemies, items, kamera, history
            if (hasSnap)
                SaveManager::ApplyPreSpawn(snap);

            SpawnEnemiesFromMap();
            SpawnItemWave();

            if (hasSnap)
            {
                Entities::Add(&PlayerInstance);
                SaveManager::ApplyPostSpawn(snap);
            }
            else
            {
                Entities::Add(&PlayerInstance);
            }

            // ── Rebuild collision & flow field ──
            SpawnObject();
            RebuildObstacleCache();
            globalFlowField.Invalidate();

            // ── JANGAN CaptureSnapshot/SaveInitial — snapshot immutable ──

            state->currentScreen = PLAY;
            Hide();
        }
        return;
    }

    for (std::uint8_t i = 0; i < 7; i++)
    {
        if (buttons[i].isClicked(mousePosition, mouseClicked))
        {
            HandleButtonClick(i, state);
        }
    }
}

/**
 * @brief Me-render pause menu ke layar
 * @param mousePosition Posisi mouse untuk efek hover
 */
void PauseMenu::Draw(Vector2 mousePosition)
{
    if (!active)
    {
        return;
    }

    Rectangle fullScreen = {0, 0, static_cast<float>(GameScreenWidth), static_cast<float>(GameScreenHeight)};
    Color dimColor = {0, 0, 0, static_cast<unsigned char>(255 * 0.2F)};
    DrawRectangleRec(fullScreen, dimColor);

    DrawTextureV(bgTexture, position, WHITE);

    for (std::uint8_t i = 0; i < 7; i++)
    {
        buttons[i].Draw(mousePosition);
    }

    if (savePopup.IsActive())
    {
        savePopup.Draw(mousePosition);
    }
    if (saveErrorPopup.IsActive())
    {
        saveErrorPopup.Draw(mousePosition);
    }
    if (loadConfirmPopup.IsActive())
    {
        loadConfirmPopup.Draw(mousePosition);
    }
    if (noSavePopup.IsActive())
    {
        noSavePopup.Draw(mousePosition);
    }
    if (pauseCorruptPopup.IsActive())
    {
        pauseCorruptPopup.Draw(mousePosition);
    }
    if (returnConfirmPopup.IsActive())
    {
        returnConfirmPopup.Draw(mousePosition);
    }
    if (restartConfirmPopup.IsActive())
    {
        restartConfirmPopup.Draw(mousePosition);
    }
}
