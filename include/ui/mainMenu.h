#pragma once

/**
 * @file mainmenu.h
 * @brief Main Menu System Module
 * 
 * Handle semua fungsi main menu: init, update, dan render.
 * Pake buttonTxt buat tombol-tombol menu.
 */

#include "buttonTxt.h"
#include "screen.h"
#include <cstdint>

// ================================================================
// Main Menu Functions
// ================================================================

/*==============================================================================
 * Menu Functions
 *==============================================================================*/

/**
 * @brief Inisialisasi main menu
 * @param state GameState pointer - buat akses ke data game
 * @note Bikin dan setup semua button menu (Start, Load, Options, Quit)
 */
void InitMainMenu(GameState* state);

/**
 * @brief Update logic main menu
 * @param state GameState pointer - buat akses ke input dan state game
 * @note Handle input dan navigasi menu tiap frame
 */
void UpdateMainMenu(GameState* state);

/**
 * @brief Render main menu ke virtual screen
 * @param state GameState pointer - buat akses ke button dan UI state
 * @note Render semua tombol menu ke virtual screen (bukan langsung ke layar)
 */
void RenderMainMenuToVirtualScreen(GameState* state);

/*==============================================================================
 * Menu Button Enum
 *==============================================================================*/

/**
 * @brief Enum buat jenis-jenis tombol di main menu
 * @note Dipake buat identifikasi tombol yang diklik atau aktif
 */
enum MenuButton : std::uint8_t {
    BTN_START = 0,   /**< Tombol Start Game - mulai game baru */
    BTN_LOAD,        /**< Tombol Load Game - load save game */
    BTN_OPTIONS,     /**< Tombol Options - buka menu pengaturan */
    BTN_QUIT         /**< Tombol Quit - keluar dari game */
};