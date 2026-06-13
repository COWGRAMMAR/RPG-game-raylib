/**
 * @file pauseMenu.cpp
 * @brief Implementasi dari Pause Menu System dan Options Screen
 *
 * Handle pause menu UI dan standalone options screen dengan tabs.
 */

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "../../include/systems/keybindManager.h"
#include "../../include/rendering/fonts.h"
#include "../../include/ui/pauseMenu.h"
#include "../../include/ui/popup.h"
#include "../../include/ui/videoTab.h"
#include "../../include/ui/audioTab.h"
#include "../../include/ui/keybindsTab.h"
#include "../../include/ui/saveLoadScreen.h"
#include "../../include/core/game_state_saver.h"
#include "../../include/core/savemanager.h"
#include "../../include/map/worldgenio.h"
#include "../../include/core/seedmanager.h"
#include "entities.h"
#include "item.h"
#include "propsbehavior.h"
#include "enemy.h"
#include "enemy_ai.h"
#include "map.h"
#include "mapLogic.h"
#include "../../include/systems/combatTurn.h"

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
static Popup pauseCorruptPopup("Save file corrupted or unreadable.", "OK", 0.7f);
static Popup returnConfirmPopup("Return to main menu?", "Continue", "Cancel", 0.7f);
static Popup restartConfirmPopup("Restart Run?", "Restart", "Cancel", 0.7f);

static constexpr int TAB_HEIGHT = 56;
static constexpr int TAB_TOP_OFFSET = 135;
static constexpr int TAB_SPACING = 302;
static constexpr int BACK_BTN_RIGHT_OFFSET = 133;
static constexpr int BACK_BTN_BOTTOM_OFFSET = 53;
static constexpr int OPT_LABEL_FONT_SIZE = 24;
static constexpr int VALUE_RIGHT_OFFSET = 339;
static constexpr int CONTENT_TOP_OFFSET = 221;
static constexpr int ROW1_TOP = 15;
static constexpr int ROW2_TOP = 75;
static constexpr int RESET_TAB_LEFT = 60;
static constexpr int RESET_ALL_LEFT = 220;
static constexpr int RESET_BOTTOM_OFFSET = 70;
static constexpr int RESET_FONT_SIZE = 20;
static constexpr int CONTENT_INNER_PAD = 20;
static constexpr int CONTENT_BOX_Y = 200;
static constexpr int CONTENT_BOX_BOTTOM_TRIM = 323;
static constexpr int BTN_BG_PAD = 6;

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
    width = bgTexture.width > 0 ? bgTexture.width : 800;
    height = bgTexture.height > 0 ? bgTexture.height : 600;
    startX = (GScreenWidth - width) / 2;
    startY = (GScreenHeight - height) / 2;

    backgroundRect = {static_cast<float>(startX), static_cast<float>(startY), static_cast<float>(width), static_cast<float>(height)};

    int tabY = startY + TAB_TOP_OFFSET;

    const char *tabFiles[3] = {
        "assets/textures/settingsButt/settingsVideo.png",
        "assets/textures/settingsButt/settingsAudio.png",
        "assets/textures/settingsButt/settingsKeybinds.png"};
    for (int i = 0; i < 3; i++)
    {
        float cx = static_cast<float>(startX + width / 2 + (i - 1) * TAB_SPACING);
        float cy = static_cast<float>(tabY + TAB_HEIGHT / 2);
        tabButtons[i] = buttonImage(tabFiles[i], Vector2{cx, cy}, 1.0F, 0.7F);
    }

    backButton = buttonImage(
        "assets/textures/settingsButt/settingsBack.png",
        Vector2{static_cast<float>(startX + width - BACK_BTN_RIGHT_OFFSET),
                static_cast<float>(startY + height - BACK_BTN_BOTTOM_OFFSET)},
        1.0F, 0.7F);

    if (resolutionOptions.empty())
    {
        resolutionOptions = GetAvailableResolutions();
    }

    int valueX = startX + VALUE_RIGHT_OFFSET;
    int contentStartY = startY + CONTENT_TOP_OFFSET;

    bool isFullscreen = IsWindowFullscreen();
    fullscreenButton = buttonTxt(
        isFullscreen ? "ON" : "OFF",
        valueX,
        contentStartY + ROW1_TOP,
        OPT_LABEL_FONT_SIZE,
        isFullscreen ? GREEN : RED,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));

    fpsButton = buttonTxt(
        showFPS ? "ON" : "OFF",
        valueX,
        contentStartY + ROW2_TOP,
        OPT_LABEL_FONT_SIZE,
        showFPS ? GREEN : RED,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));

    // tombol Reset Tab / Reset All pakai GetOrLoad(FontId::LOADING_TITLE) (bold)
    resetTabButton = buttonTxt(
        "Reset Tab",
        startX + RESET_TAB_LEFT,
        startY + height - RESET_BOTTOM_OFFSET,
        RESET_FONT_SIZE,
        ORANGE,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));

    resetOptionsButton = buttonTxt(
        "Reset All",
        startX + RESET_ALL_LEFT,
        startY + height - RESET_BOTTOM_OFFSET,
        RESET_FONT_SIZE,
        ORANGE,
        0.7F,
        GetOrLoad(FontId::LOADING_TITLE));
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
            VIDEO_SETTINGS_PATH,
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
            VIDEO_SETTINGS_PATH,
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

    int newStartX = (GScreenWidth - width) / 2;
    int newStartY = (GScreenHeight - height) / 2;
    if (newStartX != startX || newStartY != startY) {
        startX = newStartX;
        startY = newStartY;
        backgroundRect = {static_cast<float>(startX), static_cast<float>(startY), static_cast<float>(width), static_cast<float>(height)};

        int tabY = startY + TAB_TOP_OFFSET;
        for (int i = 0; i < 3; i++) {
            float cx = static_cast<float>(startX + width / 2 + (i - 1) * TAB_SPACING);
            float cy = static_cast<float>(tabY + TAB_HEIGHT / 2);
            tabButtons[i].SetPosition(Vector2{cx, cy});
        }
        backButton.SetPosition(Vector2{static_cast<float>(startX + width - BACK_BTN_RIGHT_OFFSET),
                                       static_cast<float>(startY + height - BACK_BTN_BOTTOM_OFFSET)});

        int valueX = startX + VALUE_RIGHT_OFFSET;
        int contentStartY = startY + CONTENT_TOP_OFFSET;
        bool isFullscreen = IsWindowFullscreen();
        fullscreenButton = buttonTxt(
            isFullscreen ? "ON" : "OFF",
            valueX, contentStartY + ROW1_TOP, OPT_LABEL_FONT_SIZE,
            isFullscreen ? GREEN : RED, 0.7F, GetOrLoad(FontId::LOADING_TITLE));
        fpsButton = buttonTxt(
            showFPS ? "ON" : "OFF",
            valueX, contentStartY + ROW2_TOP, OPT_LABEL_FONT_SIZE,
            showFPS ? GREEN : RED, 0.7F, GetOrLoad(FontId::LOADING_TITLE));

        resetTabButton.SetPosition(Vector2{static_cast<float>(startX + RESET_TAB_LEFT),
                                           static_cast<float>(startY + height - RESET_BOTTOM_OFFSET)});
        resetOptionsButton.SetPosition(Vector2{static_cast<float>(startX + RESET_ALL_LEFT),
                                               static_cast<float>(startY + height - RESET_BOTTOM_OFFSET)});
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
        static_cast<float>(startX + contentOX + CONTENT_INNER_PAD),
        static_cast<float>(startY + CONTENT_BOX_Y),
        static_cast<float>(width - 2 * (contentOX + CONTENT_INNER_PAD)),
        static_cast<float>(height - CONTENT_BOX_BOTTOM_TRIM)};
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

    // background hitam di belakang tombol Reset Tab / Reset All agar terbaca
    {
        Rectangle tabRect = resetTabButton.GetBounds();
        Rectangle resetAllRect = resetOptionsButton.GetBounds();
        int pad = BTN_BG_PAD;
        DrawRectangle(
            static_cast<int>(tabRect.x) - pad,
            static_cast<int>(tabRect.y) - pad,
            static_cast<int>(tabRect.width) + pad * 2,
            static_cast<int>(tabRect.height) + pad * 2,
            BLACK);
        DrawRectangle(
            static_cast<int>(resetAllRect.x) - pad,
            static_cast<int>(resetAllRect.y) - pad,
            static_cast<int>(resetAllRect.width) + pad * 2,
            static_cast<int>(resetAllRect.height) + pad * 2,
            BLACK);
    }

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
    // offset khusus returnConfirmPopup agar teks & tombol sejajar
    returnConfirmPopup.SetTextYOffset(15);
    returnConfirmPopup.SetButtonYOffset(-15);

    Image img = LoadImage("assets/textures/pauseButt/pause-bg.png");
    bgTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    width = bgTexture.width;
    height = bgTexture.height;

    for (uint8_t i = 0; i < 7; i++)
        buttons[i] = buttonImage(BUTTON_PATHS[i], Vector2{0, 0}, 1.0F, 0.6F);

    CalculateDimensions();
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

void PauseMenu::CalculateDimensions()
{
    position.x = (GScreenWidth - width) / 2.0F;
    position.y = (GScreenHeight - height) / 2.0F;
    backgroundRect = {position.x, position.y, static_cast<float>(width), static_cast<float>(height)};

    float centerX = position.x + width / 2.0F;
    float gap = 28.0F;
    float btnHeight = 56.0F;
    float pairGap = 105.0F;
    float totalBtnHeight = 5.0F * btnHeight + 4.0F * gap;
    float startY = position.y + (height - totalBtnHeight) / 2.0F + 65.0F;

    // Row 0: Resume (wide, centered)
    {
        float btnY = startY + btnHeight / 2.0F;
        buttons[0].SetPosition(Vector2{centerX, btnY});
    }

    // Row 1: Save (left) / Load (right)
    {
        float btnY = startY + 1.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[1].SetPosition(Vector2{centerX - pairGap, btnY});
        buttons[2].SetPosition(Vector2{centerX + pairGap, btnY});
    }

    // Row 2: Settings (wide, centered)
    {
        float btnY = startY + 2.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[3].SetPosition(Vector2{centerX, btnY});
    }

    // Row 3: Restart (left) / To Main (right)
    {
        float btnY = startY + 3.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[4].SetPosition(Vector2{centerX - pairGap, btnY});
        buttons[5].SetPosition(Vector2{centerX + pairGap, btnY});
    }

    // Row 4: Exit (wide, centered)
    {
        float btnY = startY + 4.0F * (btnHeight + gap) + btnHeight / 2.0F;
        buttons[6].SetPosition(Vector2{centerX, btnY});
    }
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
        SaveGameState(state);
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
            if (ReadSaveFile(GetSlotPath(g_ActiveSaveSlot, "manual")))
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
                DeleteSaveFile(GetSlotPath(g_ActiveSaveSlot, "manual"));
                pauseCorruptPopup.Show();
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
        return;
    }

    if (returnConfirmPopup.IsActive())
    {
        returnConfirmPopup.Update(mousePosition, mouseClicked);
        if (returnConfirmPopup.IsConfirmClicked())
        {
            InputInstance.ResetMenuFlags();
            state->enteredLoading = false;
            state->loadingStage = 0;
            state->loadingProgress = 0.0F;
            state->loadingComplete = false;
            state->currentScreen = MAIN_MENU;
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
            // Clear semua runtime state
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

            // Load initial snapshot untuk restore state enemies & items
            // ApplyPreSpawn HARUS sebelum SpawnEnemiesFromMap agar dead entities
            // sudah terdaftar sebelum enemy spawn logic berjalan
            {
                GameSnapshot initialSnap;
                bool hasInitial = SaveManager::HasInitial(g_ActiveSaveSlot);
                if (hasInitial)
                {
                    initialSnap = SaveManager::LoadInitial(g_ActiveSaveSlot);
                    SaveManager::ApplyPreSpawn(initialSnap);
                }

                // Spawn enemies — dead entities sudah diset oleh ApplyPreSpawn
                SpawnEnemiesFromMap();

                // Fallback: spawn item fresh
                SpawnItemWave();

                // Apply initial state on top
                if (hasInitial)
                    SaveManager::ApplyCheckpointData(initialSnap);
            }

            // Reset & reposition player
            PlayerInstance.ResetForNewGame();
            PlayerInstance.Init(state, SPAWN_OBJECT_NAME);
            TiledHelperFunction.TryGetObjectPositionByName(SPAWN_OBJECT_NAME, state->startSpawnPos);
            PlayerInstance.hasDroppedItems = false;
            Entities::Add(&PlayerInstance);

            // Reset camera ke posisi player
            Vector2 spawnPos = PlayerInstance.GetPosition();
            camera.target = {spawnPos.x + (FRAME_SIZE / 2.0F), spawnPos.y + (FRAME_SIZE / 2.0F)};
            camera.offset = {(float)(GScreenWidth / 2), (float)(GScreenHeight / 2)};
            camera.rotation = 0;
            camera.zoom = 1.0F;

            // Re-init world objects & collision
            SpawnObject();
            RebuildObstacleCache();
            globalFlowField.Invalidate();

            // Re-capture initial state untuk restart berikutnya
            {
                GameSnapshot freshInitial = SaveManager::CaptureSnapshot();
                SaveManager::SaveInitial(freshInitial, g_ActiveSaveSlot);
            }

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

    int expectedX = (GScreenWidth - width) / 2;
    if (expectedX != static_cast<int>(position.x))
    {
        CalculateDimensions();
    }

    Rectangle fullScreen = {0, 0, static_cast<float>(GScreenWidth), static_cast<float>(GScreenHeight)};
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
