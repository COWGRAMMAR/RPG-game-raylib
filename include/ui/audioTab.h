#pragma once

/**
 * @file audioTab.h
 * @brief Audio Settings Tab for Options Screen
 *
 * Contains volume controls (sliders) and audio settings persistence.
 */

#include "../lib/raylib/include/raylib.h"
#include "buttonTxt.h"
#include "../systems/audioManager.h"

static const char* AUDIO_SETTINGS_PATH = "saves/settings/audioTab.json";

/**
 * @brief State untuk slider volume di tab Audio
 */
struct SliderState {
    int masterVolume;   ///< Volume master 0-100
    int musicVolume;    ///< Volume musik 0-100
    int sfxVolume;      ///< Volume SFX 0-100
    bool isDragging;    ///< true saat user sedang drag slider
    int dragIndex;      ///< Index slider yang di-drag (0=master, 1=music, 2=sfx)
};

/** @brief Global slider state (defined in audioTab.cpp) */
extern SliderState g_sliders;

/**
 * @brief Me-render tab Audio
 * @param mousePosition Posisi mouse untuk efek hover
 * @param startX Posisi X awal area options
 * @param startY Posisi Y awal area options
 */
void DrawAudioTab(
    Vector2 mousePosition,
    int startX,
    int startY
);

/**
 * @brief Memperbarui handling input untuk slider volume di tab Audio
 * @param sliders State slider yang akan diperbarui
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 * @param startX Posisi X awal area options
 * @param startY Posisi Y awal area options
 * @return true jika ada perubahan pengaturan yang disimpan
 */
bool UpdateAudioTab(
    SliderState& sliders,
    Vector2 mousePosition,
    bool mouseClicked,
    int startX,
    int startY
);

/**
 * @brief Memuat pengaturan audio dari file JSON
 * @return true jika berhasil dimuat
 */
bool LoadAudioSettings();

/**
 * @brief Menyimpan pengaturan audio ke file JSON
 * @param masterVolume Volume master (0-100)
 * @param musicVolume Volume musik (0-100)
 * @param sfxVolume Volume SFX (0-100)
 * @return true jika berhasil disimpan
 */
bool SaveAudioSettings(int masterVolume, int musicVolume, int sfxVolume);