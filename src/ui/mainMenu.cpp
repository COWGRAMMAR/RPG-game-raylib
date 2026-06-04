/**
 * @file mainMenu.cpp
 * @brief Implementasi dari Main Menu System Module
 *
 * Implementasi dari fungsi-fungsi main menu yang dideklarasikan di mainMenu.h
 * Handle inisialisasi tombol, update input, dan rendering menu utama.
 */

#include "mainMenu.h"
#include "screen.h"
#include "seedmanager.h"
#include "worldgenio.h"
#include "../lib/raylib/include/raylib.h"
#include <array>

/*==============================================================================
 * Static Variables
 *==============================================================================*/

/** Array tombol menu utama (Start, Load, Options, Quit) */
static std::array<buttonTxt, 4> buttons;

/** Logo texture untuk main menu */
static Texture2D logoTexture;

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

    // Load dan resize logo
    Image logoImg = LoadImage("assets/textures/logo.png");
    int targetWidth = static_cast<int>(3840 * 0.13F);
    int targetHeight = static_cast<int>(2160 * 0.13F);
    ImageResize(&logoImg, targetWidth, targetHeight);
    logoTexture = LoadTextureFromImage(logoImg);
    UnloadImage(logoImg);

    // Daftar teks tombol sesuai urutan enum MenuButton
    std::array<const char *, 4> texts = {"Start Game", "Load Game", "Options", "Quit"};

    // Hitung posisi tengah layar virtual
    int centerX = (GameScreenWidth / 2) - 50;
    int startY = (GameScreenHeight / 2) + 20;
    int buttonSpacing = 70;
    int fontSize = 30;

    // Looping bilang 4 tombol dengan posisi vertikal berurutan
    for (int i = 0; i < 4; i++)
    {
        buttons[i] = buttonTxt(texts[i], centerX, startY + (i * buttonSpacing), fontSize, WHITE, 0.6F);
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
                    state->currentScreen = LOADING;
                    break;
                case 2:  // Options
                    state->previousScreen = MAIN_MENU;
                    state->currentScreen = OPTIONS;
                    break;
                case 3:  // Quit
                    CloseWindow();
                    break;
                case 1:  // Load Game — load dari top slot
                {
                    int slot = WorldgenIO::GetTopSlot();
                    if (slot > 0)
                    {
                        std::string metaPath = WorldgenIO::GetMetaPath(slot);
                        if (g_SeedManager.LoadMeta(metaPath))
                        {
                            state->pendingMapPath = WorldgenIO::GetStagePath(g_SeedManager.GetCurrentStage());
                            state->pendingDoorName = "start";
                            state->isSwitchingMap = true;
                            state->currentScreen = LOADING;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
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
    ClearBackground(DARKGRAY);

    // Render logo
    int logoX = (GameScreenWidth / 2) - (logoTexture.width / 2);
    DrawTexture(logoTexture, logoX, 60, WHITE);

    // Render semua tombol menu
    for (int i = 0; i < 4; i++)
    {
        buttons[i].Draw(virtualMouse);
    }

    EndTextureMode();
}
