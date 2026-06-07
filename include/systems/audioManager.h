#pragma once

/**
 * @file audioManager.h
 * @brief Audio Manager Module
 *
 * Mengelola volume (Master, Music, SFX) dan playback background music
 * per screen state (MAIN_MENU, PLAY, GAME_OVER). Settings dipersist
 * ke JSON lewat audioTab, sementara AudioManager menangani runtime.
 */

#include "../lib/raylib/include/raylib.h"
#include "../core/screen.h"

/**
 * @brief Audio Manager — namespace dengan free functions
 *
 * Arsitektur:
 * - Volume disimpan sebagai float 0.0f-1.0f, ditampilkan sebagai persen 0-100
 * - Music track otomatis berganti sesuai ScreenState
 * - LOADING dan OPTIONS tidak memicu switch track (lanjut track sebelumnya)
 */
namespace AudioManager {

/*------------------------------------------------------------------------------
 * Lifecycle
 *------------------------------------------------------------------------------*/

/**
 * @brief Inisialisasi AudioManager dengan volume default
 *
 * Set master=1.0f, music=0.8f, sfx=1.0f, panggil ::SetMasterVolume().
 * Wajib dipanggil setelah InitAudioDevice().
 */
void Init();

/**
 * @brief Memuat semua aset audio (music + SFX) dari disk
 *
 * Load 5 music tracks dari assets/audio/music/ dan 1 SFX dari
 * assets/audio/sfx/. Setiap kegagalan load di-log via TraceLog.
 */
void LoadAudioAssets();

/**
 * @brief Membersihkan semua aset audio
 *
 * Unload semua Music stream dan Sound. Panggil sebelum CloseAudioDevice().
 */
void Shutdown();

/*------------------------------------------------------------------------------
 * Per-frame Update
 *------------------------------------------------------------------------------*/

/**
 * @brief Update per-frame untuk audio system
 *
 * - Memanggil UpdateMusicStream() pada track yang sedang aktif
 * - Menerapkan volume (musicVolume * masterVolume) setiap frame
 * - Mendeteksi perubahan screen untuk auto-switch track
 *
 * @param currentScreen Screen state saat ini
 */
void Update(ScreenState currentScreen);

/*------------------------------------------------------------------------------
 * Volume Getters (float)
 *------------------------------------------------------------------------------*/

/** @brief Mendapatkan volume master (0.0f-1.0f) */
float GetMasterVolume();

/** @brief Mendapatkan volume musik (0.0f-1.0f) */
float GetMusicVolume();

/** @brief Mendapatkan volume SFX (0.0f-1.0f) */
float GetSfxVolume();

/*------------------------------------------------------------------------------
 * Volume Setters (float, otomatis clamp ke [0.0f, 1.0f])
 *------------------------------------------------------------------------------*/

/** @brief Mengatur volume master dan memanggil ::SetMasterVolume() */
void SetMasterVolume(float vol);

/** @brief Mengatur volume musik */
void SetMusicVolume(float vol);

/** @brief Mengatur volume SFX */
void SetSfxVolume(float vol);

/*------------------------------------------------------------------------------
 * Percentage Convenience (0-100)
 *------------------------------------------------------------------------------*/

/** @brief Mendapatkan volume master dalam persen (0-100) */
int GetMasterVolumePct();

/** @brief Mendapatkan volume musik dalam persen (0-100) */
int GetMusicVolumePct();

/** @brief Mendapatkan volume SFX dalam persen (0-100) */
int GetSfxVolumePct();

/**
 * @brief Mengatur semua volume dari nilai persen
 * @param masterPct Volume master 0-100
 * @param musicPct  Volume musik 0-100
 * @param sfxPct    Volume SFX 0-100
 */
void SetVolumesFromPct(int masterPct, int musicPct, int sfxPct);

/*------------------------------------------------------------------------------
 * Music Control
 *------------------------------------------------------------------------------*/

/**
 * @brief Memutar track music berdasarkan nama file
 * @param trackName Nama file atau prefix untuk mencari track
 */
void PlayTrack(const char* trackName);

/** @brief Menghentikan semua music playback */
void StopMusic();

/*------------------------------------------------------------------------------
 * SFX Control
 *------------------------------------------------------------------------------*/

/**
 * @brief Memainkan efek suara (SFX) untuk serangan player
 *
 * Volume SFX dipengaruhi oleh _sfxVolume * _masterVolume.
 * Aman dipanggil dari combat.cpp saat player melee attack.
 */
void PlaySfx();

}  // namespace AudioManager
