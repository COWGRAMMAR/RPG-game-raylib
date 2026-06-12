#pragma once
#include "raylib.h"
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

/**
 * @brief Render boss HP bar di tengah bawah layar.
 * Cari enemy aktif dengan rank ENEMY_BOSS, tampilkan nama + bar + HP%.
 */
void DrawBossHPBar();

/**
 * @brief Update boss music ambient per-frame.
 *
 * Deteksi player dalam detection range boss ATAU di prefab room CELL_BOSS.
 * Overwrite current track pake PlayTrack(5), balikin pake ResetToScreenTrack.
 */
void UpdateBossMusic();
