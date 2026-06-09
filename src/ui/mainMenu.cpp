/**
 * @file mainMenu.cpp
 * @brief Implementasi dari Main Menu System Module
 *
 * Implementasi dari fungsi-fungsi main menu yang dideklarasikan di mainMenu.h
 * Handle inisialisasi tombol, update input, dan rendering menu utama.
 */

#include "../../include/ui/mainMenu.h"
#include "../../include/core/screen.h"
#include "../../include/core/seedmanager.h"
#include "../../include/map/worldgenio.h"
#include "../../lib/raylib/include/raylib.h"
#include <array>
#include <filesystem>
#include "../../include/ui/popup.h"
#include "../../include/core/game_state_saver.h"
#include "../../include/entities/entities.h"

/*==============================================================================
 * Static Variables
 *==============================================================================*/

/** Array tombol menu utama (Start, Load, Options, Quit) */
static std::array<buttonImage, 4> buttons;

/** Logo texture untuk main menu */
static Texture2D logoTexture;

/** Save/Load popups */
static Popup startNewPopup("Start new game? Current save will be lost.", "Start New", "Cancel", 0.7f);
static Popup loadPopup("Load saved game?", "Load Save", "Cancel", 0.7f);
static Popup mainNoSavePopup("No save file found.", "OK", 0.7f);
static Popup mainCorruptPopup("Save file corrupted or unreadable.", "OK", 0.7f);
static bool waitingStartConfirm = false;
static bool waitingLoadConfirm = false;

/*==============================================================================
 * Public Functions
 *==============================================================================*/

/**
 * @brief InitMainMenu()
 * Inisialisasi semua tombol menu utama dalam array.
 * @param state GameState pointer (dipake buat akses screen size, tapi sementara di-ignore)
 */
void InitMainMenu(GameState *state)
{
    (void)state; // unused parameter, buat future use

    // set texture background load-notif untuk semua popup main menu
    startNewPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    loadPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    mainNoSavePopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");
    mainCorruptPopup.SetBackgroundTexture("assets/textures/pauseButt/load-notif.png");

    // Load dan resize logo
    Image logoImg = LoadImage("assets/textures/logo.png");
    int targetWidth = static_cast<int>(3840 * 0.25F);
    int targetHeight = static_cast<int>(2160 * 0.25F);
    ImageResize(&logoImg, targetWidth, targetHeight);
    logoTexture = LoadTextureFromImage(logoImg);
    UnloadImage(logoImg);

    // Konfigurasi individu untuk setiap tombol (path, scale, yOffset)
    struct { const char *path; float scale; int yOffset; } btnConfig[4] = {
        {"assets/textures/mainMenuButt/main-start.png",    1, -40},
        {"assets/textures/mainMenuButt/main-load.png",     1,  50},
        {"assets/textures/mainMenuButt/main-settings.png", 1, 140},
        {"assets/textures/mainMenuButt/main-quit.png",     1, 230},
    };

    int startY = (GameScreenHeight / 2) + 20;

    for (int i = 0; i < 4; i++)
    {
        Vector2 pos = {
            GameScreenWidth / 2.0F,
            static_cast<float>(startY + btnConfig[i].yOffset)
        };
        buttons[i] = buttonImage(btnConfig[i].path, pos, btnConfig[i].scale, 0.6F);
    }
}

/**
 * @brief UpdateMainMenu()
 * Tangani input mouse dan klik tombol untuk navigasi menu.
 * @param state GameState pointer - buat ngubah currentScreen
 */
void UpdateMainMenu(GameState *state)
{
    Vector2 mousePosition = GetVirtualMousePosition(state);
    bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    for (int i = 0; i < 4; i++) {
        if (buttons[i].isClicked(mousePosition, mouseClicked)) {
            switch (i) {
                case 0:  // Start Game
                    if (HasSaveFile("saves/manual/slot0.json"))
                    {
                        waitingStartConfirm = true;
                        startNewPopup.Show();
                    }
                    else
                    {
                        // Bersihkan per-map state dari sesi sebelumnya
                        std::filesystem::remove_all("saves/enemies");
                        std::filesystem::remove_all("saves/items");
                        state->enteredLoading = false;
                        state->currentScreen = LOADING;
                    }
                    break;
                case 1:  // Load Game
                    if (HasSaveFile("saves/manual/slot0.json"))
                    {
                        if (ReadSaveFile("saves/manual/slot0.json"))
                        {
                            waitingLoadConfirm = true;
                            loadPopup.Show();
                        }
                        else
                        {
                            DeleteSaveFile("saves/manual/slot0.json");
                            mainCorruptPopup.Show();
                        }
                    }
                    else
                    {
                        mainNoSavePopup.Show();
                    }
                    break;
                case 2:  // Options
                    state->previousScreen = MAIN_MENU;
                    state->currentScreen = OPTIONS;
                    break;
                case 3:  // Quit
                    CloseWindow();
                    break;
                default:
                    break;
            }
        }
    }

    // Handle Start Game confirmation
    if (waitingStartConfirm && startNewPopup.IsActive())
    {
        startNewPopup.Update(mousePosition, mouseClicked);
        if (startNewPopup.IsConfirmClicked())
        {
            DeleteSaveFile("saves/manual/slot0.json");
            ClearSavedState();
            // Reset worldgen flag for fresh start
            SetWorldgenPending(false);
            // Clean up per-map persistence from previous session so enemies/items
            // spawn fresh instead of being loaded as dead from old save files
            Entities::SetDeadEntities({});
            std::filesystem::remove_all("saves/enemies");
            std::filesystem::remove_all("saves/items");
            state->enteredLoading = false;
            state->currentScreen = LOADING;
            waitingStartConfirm = false;
        }
        else if (!startNewPopup.IsActive())
        {
            waitingStartConfirm = false;
        }
    }

    // Handle Load Game confirmation
    if (waitingLoadConfirm && loadPopup.IsActive())
    {
        loadPopup.Update(mousePosition, mouseClicked);
        if (loadPopup.IsConfirmClicked())
        {
            state->enteredLoading = false;
            state->currentScreen = LOADING;
            waitingLoadConfirm = false;
        }
        else if (!loadPopup.IsActive())
        {
            ClearSavedState();
            waitingLoadConfirm = false;
        }
    }

    // Handle no-save popup (auto-return)
    if (mainNoSavePopup.IsActive())
    {
        mainNoSavePopup.Update(mousePosition, mouseClicked);
    }

    // Handle corrupt save popup
    if (mainCorruptPopup.IsActive())
    {
        mainCorruptPopup.Update(mousePosition, mouseClicked);
    }

    // Bersihkan semua popup saat keluar dari main menu ke layar lain
    // (misal Start Game → LOADING atau Options → OPTIONS).
    // Mencegah popup yang tidak sempat di-dismiss (seperti mainNoSavePopup)
    // tetap aktif dan muncul kembali saat player kembali ke main menu.
    if (state->currentScreen != MAIN_MENU)
    {
        if (startNewPopup.IsActive()) startNewPopup.Hide();
        if (loadPopup.IsActive()) loadPopup.Hide();
        if (mainNoSavePopup.IsActive()) mainNoSavePopup.Hide();
        if (mainCorruptPopup.IsActive()) mainCorruptPopup.Hide();
    }
}

/**
 * @brief RenderMainMenuToVirtualScreen()
 * Render semua tombol menu ke layar virtual.
 * @param state GameState pointer - buat akses ke render texture
 */
void RenderMainMenuToVirtualScreen(GameState *state)
{
    Vector2 virtualMouse = GetVirtualMousePosition(state);

    // Mulai render ke texture virtual
    BeginTextureMode(state->Dungeon);
    DrawMenuBackground();

    // Render logo
    int logoX = (GameScreenWidth / 2) - (logoTexture.width / 2);
    DrawTexture(logoTexture, logoX, 10, WHITE);

    // Render semua tombol menu
    for (int i = 0; i < 4; i++)
    {
        buttons[i].Draw(virtualMouse);
    }

    // Render popups
    if (startNewPopup.IsActive()) {
        startNewPopup.Draw(virtualMouse);
    }
    if (loadPopup.IsActive()) {
        loadPopup.Draw(virtualMouse);
    }
    if (mainNoSavePopup.IsActive()) {
        mainNoSavePopup.Draw(virtualMouse);
    }
    if (mainCorruptPopup.IsActive()) {
        mainCorruptPopup.Draw(virtualMouse);
    }

    EndTextureMode();
}
