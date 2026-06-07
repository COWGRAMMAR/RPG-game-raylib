#pragma once

/**
 * @file loading_screen.h
 * @brief Loading Screen Module
 *
 * Handle tampilan loading screen dan urutan loading aset.
 * Menampilkan progress sambil memuat aset game sebelum
 * transisi ke gameplay.
 */

#include "../lib/raylib/include/raylib.h"
#include "screen.h"
#include "animation.h"

/*==============================================================================
 * Loading Screen Functions
 *==============================================================================*/

/**
 * @brief Inisialisasi state loading screen
 * @param state Pointer ke GameState
 */
void InitLoadingScreen(GameState *state);

/**
 * @brief Update logic loading screen tiap frame
 * @param state Pointer ke GameState
 */
void UpdateLoadingScreen(GameState *state);

/**
 * @brief Render loading screen ke virtual screen
 * @param state Pointer ke GameState
 */
void RenderLoadingScreen(GameState *state);

/**
 * @brief Cek apakah loading sudah selesai
 * @param state Pointer ke GameState
 * @return true jika loading selesai
 */
bool IsLoadingComplete(GameState *state);
