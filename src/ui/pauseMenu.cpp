/**
 * @file pauseMenu.cpp
 * @brief Implementasi dari Pause Menu System dan Options Screen
 *
 * Handle pause menu UI dan standalone options screen dengan tabs.
 */

#include <algorithm>

#include "pauseMenu.h"
#include "popup.h"
#include "videoTab.h"
#include "audioTab.h"
#include "keybindsTab.h"
#include "game_state_saver.h"
#include "worldgenio.h"
#include "seedmanager.h"

/*==============================================================================
 * Static Variables (Popup Notifications)
 *==============================================================================*/

/**
 * @brief Popup notifikasi untuk save game
 */
static Popup savePopup("Game Saved!", "OK", 0.7F);

/**
 * @brief Popup notifikasi untuk load game
 */
static Popup loadPopup("Game Loaded!", "OK", 0.7F);

/*==============================================================================
 * OptionsScreen Implementation
 *==============================================================================*/

/**
 * @brief Constructor
 * 
 * Menginisialisasi semua tombol tab, tombol back, dan dimensi awal.
 */
OptionsScreen::OptionsScreen() : active(false), returnScreen(PLAY), selectedTab(0), showFPS(false), width(0), height(0), startX(0), startY(0)
{
    tabButtons = {buttonTxt("VIDEO", 0, 0, 30, WHITE, 0.7F),
                buttonTxt("AUDIO", 0, 0, 30, WHITE, 0.7F),
                buttonTxt("KEYBINDS", 0, 0, 30, WHITE, 0.7F)};
    backButton = buttonTxt("BACK", 0, 0, 30, WHITE, 0.7F);
}

/**
 * @brief Destructor
 */
OptionsScreen::~OptionsScreen()
{
}

/**
 * @brief Menampilkan layar options
 */
void OptionsScreen::Show()
{
    active = true;
    CalculateDimensions();
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

    if (1280 <= maxWidth && 720 <= maxHeight) {
        options.push_back({1280, 720, "720p"});
    }
    if (1920 <= maxWidth && 1080 <= maxHeight) {
        options.push_back({1920, 1080, "1080p"});
    }
    if (2560 <= maxWidth && 1440 <= maxHeight) {
        options.push_back({2560, 1440, "1440p"});
    }
    if (3840 <= maxWidth && 2160 <= maxHeight) {
        options.push_back({3840, 2160, "4K"});
    }

    if (options.empty()) {
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
    const int fontSize = 30;
    const int padding = 20;
    const int tabWidth = 200;
    const int tabSpacing = 10;

    width = 800;
    height = 600;
    startX = (GameScreenWidth - width) / 2;
    startY = (GameScreenHeight - height) / 2;

    backgroundRect = {static_cast<float>(startX), static_cast<float>(startY), static_cast<float>(width), static_cast<float>(height)};

    int tabStartX = startX + padding;
    for (int i = 0; i < 3; i++) {
        tabButtons[i] = buttonTxt(
            i == 0 ? "VIDEO" : (i == 1 ? "AUDIO" : "KEYBINDS"),
            tabStartX + (i * (tabWidth + tabSpacing)),
            startY + padding,
            fontSize,
            (selectedTab == i) ? YELLOW : WHITE,
            0.7F);
    }

    int backWidth = MeasureText("BACK", fontSize);
    backButton = buttonTxt(
        "BACK", 
        startX + width - backWidth - padding - 20, 
        startY + height - fontSize - padding - 20, 
        fontSize, WHITE, 0.7F);

    if (resolutionOptions.empty()) {
        resolutionOptions = GetAvailableResolutions();
    }

    const int labelFontSize = 24;
    int labelX = startX + 40;
    int valueX = startX + 250;
    int contentStartY = startY + 100;

    bool isFullscreen = IsWindowFullscreen();
    fullscreenButton = buttonTxt(
        isFullscreen ? "ON" : "OFF",
        valueX,
        contentStartY + 15,
        labelFontSize,
        isFullscreen ? GREEN : RED,
        0.7F);

fpsButton = buttonTxt(
        showFPS ? "ON" : "OFF",
        valueX,
        contentStartY + 75,
        labelFontSize,
        showFPS ? GREEN : GRAY,
        0.7F);
}

/**
 * @brief Memperbarui handling input
 * @param state Pointer ke GameState
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 */
void OptionsScreen::Update(GameState* state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active) {
        return;
}

    if (backButton.isClicked(mousePosition, mouseClicked)) {
        active = false;
        state->currentScreen = returnScreen;
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (tabButtons[i].isClicked(mousePosition, mouseClicked)) {
            selectedTab = i;
            CalculateDimensions();
            return;
        }
    }

    if (selectedTab == 0) {
        if (UpdateVideoTab(fullscreenButton, fpsButton, state, mousePosition, mouseClicked)) {
            showFPS = state->showFPS;
            CalculateDimensions();
        }
    }
}

/**
 * @brief Me-render layar options
 * @param mousePosition Posisi mouse untuk efek hover
 */
void OptionsScreen::Draw(Vector2 mousePosition)
{
    if (!active) {
        return;
    }

    Color bgColor = {40, 40, 40, 230};
    DrawRectangleRec(backgroundRect, bgColor);
    DrawRectangleLinesEx(backgroundRect, 2, WHITE);

    for (int i = 0; i < 3; i++) {
        tabButtons[i].Draw(mousePosition);
    }

    backButton.Draw(mousePosition);

    switch (selectedTab) {
        case 0: DrawVideoTab(fullscreenButton, fpsButton, mousePosition, startX, startY); break;
        case 1: DrawAudioTab(mousePosition, startX, startY); break;
        case 2: DrawKeybindsTab(mousePosition, startX, startY); break;
    }
}

/*==============================================================================
 * PauseMenu Implementation
 *==============================================================================*/

/**
 * @brief Constructor
 * 
 * Menginisialisasi semua tombol menu: Resume, Save, Load, Options, Return, Close
 */
PauseMenu::PauseMenu() : active(false), position({0, 0}), width(0), height(0)
{
    buttonTexts = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    buttonTexts[0] = "Resume";
    buttonTexts[1] = "Save Game";
    buttonTexts[2] = "Load Game";
    buttonTexts[3] = "Options";
    buttonTexts[4] = "Return to Main Menu";
    buttonTexts[5] = "Close Game";

    CalculateDimensions();
}

/**
 * @brief Destructor
 */
PauseMenu::~PauseMenu()
{
}

/**
 * @brief Menampilkan pause menu
 */
void PauseMenu::Show()
{
    CalculateDimensions();
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
 * @brief Menghitung dimensi menu berdasarkan layar
 */
void PauseMenu::CalculateDimensions()
{
    const int maxWidth = static_cast<int>(GameScreenWidth * 0.30F);
    const int fontSize = 30;
    const int paddingY = 20;
    const int buttonSpacing = 10;
    const int separatorSpacing = 30;

    int maxButtonWidth = 0;
    for (std::uint8_t i = 0; i < 6; i++) {
        int btnWidth = MeasureText(buttonTexts[i], fontSize);
        maxButtonWidth = std::max(btnWidth, maxButtonWidth);
    }

    width = maxButtonWidth;
    height = GameScreenHeight;
    position.x = 0;
    position.y = 0;

    backgroundRect = {position.x, position.y, static_cast<float>(width), static_cast<float>(height)};

    for (std::uint8_t i = 0; i < 6; i++) {
        int btnWidth = MeasureText(buttonTexts[i], fontSize);
        int btnX = position.x + ((width - btnWidth) / 2);

        int separatorCount = 0;
        if (i > 0) {
            separatorCount++;
        }
        if (i > 2) {
            separatorCount++;
        }
        if (i > 4) {
            separatorCount++;
        }

        int btnY = position.y + paddingY + (i * (fontSize + buttonSpacing)) + (separatorCount * (separatorSpacing - buttonSpacing));
        buttons[i] = buttonTxt(buttonTexts[i], btnX, btnY, fontSize, WHITE, 0.7F);
    }
}

/**
 * @brief Handle klik pada tombol berdasarkan index
 * @param buttonIndex Index tombol yang diklik (0-5)
 * @param state Pointer ke GameState
 */
void PauseMenu::HandleButtonClick(int buttonIndex, GameState* state)
{
    switch (buttonIndex) {
        case 0:
            Hide();
            break;
        case 1:
            // Save Game — simpan runtime + player state ke disk
            WorldgenIO::SaveRuntimeState(g_SeedManager.GetCurrentStage());
            SaveGameState(state);
            savePopup.Show();
            break;
        case 2:
            loadPopup.Show();
            break;
        case 3:
            state->previousScreen = PLAY;
            state->currentScreen = OPTIONS;
            break;
        case 4:
            // Return to Main Menu — save dulu baru pindah
            WorldgenIO::SaveRuntimeState(g_SeedManager.GetCurrentStage());
            SaveGameState(state);
            state->enteredLoading = false;
            state->loadingStage = 0;
            state->loadingProgress = 0.0F;
            state->loadingComplete = false;
            state->currentScreen = MAIN_MENU;
            Hide();
            break;
        case 5:
            // Close Game — save dulu baru tutup
            WorldgenIO::SaveRuntimeState(g_SeedManager.GetCurrentStage());
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
void PauseMenu::Update(GameState* state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active) {
        return;
    }

    if (savePopup.IsActive()) {
        savePopup.Update(mousePosition, mouseClicked);
        return;
    }

    if (loadPopup.IsActive()) {
        loadPopup.Update(mousePosition, mouseClicked);
        return;
    }

    for (std::uint8_t i = 0; i < 6; i++) {
        if (buttons[i].isClicked(mousePosition, mouseClicked)) {
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
    if (!active) {
        return;
}

    Rectangle fullScreen = {0, 0, static_cast<float>(GameScreenWidth), static_cast<float>(GameScreenHeight)};
    Color dimColor = {0, 0, 0, static_cast<unsigned char>(255 * 0.2F)};
    DrawRectangleRec(fullScreen, dimColor);

    Color bgColor = DARKGRAY;
    DrawRectangleRec(backgroundRect, bgColor);
    DrawRectangleLines(
        static_cast<int>(backgroundRect.x),
        static_cast<int>(backgroundRect.y),
        static_cast<int>(backgroundRect.width),
        static_cast<int>(backgroundRect.height),
        WHITE);

    for (std::uint8_t i = 0; i < 6; i++) {
        buttons[i].Draw(mousePosition);
    }

    if (savePopup.IsActive()) {
        savePopup.Draw(mousePosition);
    }
    if (loadPopup.IsActive()) {
        loadPopup.Draw(mousePosition);
    }
}
