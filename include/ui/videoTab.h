#pragma once

#include "../lib/raylib/include/raylib.h"
#include "buttonTxt.h"
#include <string>

/**
 * @file videoTab.h
 * @brief Video Settings Tab for Options Screen
 * 
 * Contains fullscreen toggle and FPS display toggle.
 */

static const char* VIDEO_SETTINGS_PATH = "saves/settings/videoTab.json";

/**
 * @brief Me-render tab Video
 * @param fullscreenButton Tombol toggle fullscreen
 * @param fpsButton Tombol toggle FPS display
 * @param mousePosition Posisi mouse untuk efek hover
 * @param startX Posisi X awal area options
 * @param startY Posisi Y awal area options
 */
void DrawVideoTab(
    buttonTxt& fullscreenButton,
    buttonTxt& fpsButton,
    Vector2 mousePosition,
    int startX,
    int startY
);

/**
 * @brief Memperbarui handling input untuk tab Video
 * @param fullscreenButton Tombol toggle fullscreen
 * @param fpsButton Tombol toggle FPS display
 * @param state Pointer ke GameState
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 * @return true jika ada perubahan yang memerlukan redraw
 */
bool UpdateVideoTab(
    buttonTxt& fullscreenButton,
    buttonTxt& fpsButton,
    void* state,
    Vector2 mousePosition,
    bool mouseClicked
);

/**
 * @brief Memuat pengaturan video dari file JSON
 * @param state Pointer ke GameState untuk mengisi showFPS dll
 * @return true jika berhasil dimuat
 */
bool LoadVideoSettings(void* state);

/**
 * @brief Menyimpan pengaturan video ke file JSON
 * @param fullscreen Status fullscreen saat ini
 * @param showFPS Status tampilan FPS saat ini
 * @return true jika berhasil disimpan
 */
bool SaveVideoSettings(bool fullscreen, bool showFPS);
