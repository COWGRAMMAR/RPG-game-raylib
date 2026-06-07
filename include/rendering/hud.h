#pragma once
#include "../lib/raylib/include/raylib.h"
#include "screen.h"

/**
 * @brief Render HUD player (Nama, Health, Mana) di pojok kiri bawah.
 * Dipanggil dari DrawUIOverlay di screen_handler.cpp.
 */
void DrawPlayerHUD();

/**
 * @brief Render layar inventory beserta logika drag & drop, split, dan merge.
 */
void DrawInventory();

/**
 * @brief Render hotbar beserta logika drag & drop saat inventory terbuka.
 */
void DrawHotbar();

/**
 * @brief Render dialog sign overlay (dim + box + text).
 */
void DrawSignDialog();