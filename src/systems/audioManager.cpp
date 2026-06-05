/**
 * @file audioManager.cpp
 * @brief Implementasi Audio Manager Module
 *
 * Mengelola volume (Master, Music, SFX) menggunakan raylib audio API.
 * Memuat 5 music tracks dari assets/audio/music/ dan 1 SFX.
 * Music playback otomatis berganti sesuai ScreenState.
 */

#include "../../include/systems/audioManager.h"
#include "../../lib/raylib/include/raylib.h"
#include <cstring>

/*==============================================================================
 * Internal State (file-scope statics)
 *==============================================================================*/

/** @brief Array music tracks yang sudah di-load */
static Music _tracks[5] = {};

/** @brief Sound SFX yang sudah di-load */
static Sound _sfx = {};

static float _masterVolume = 1.0f;
static float _musicVolume  = 0.8f;
static float _sfxVolume    = 1.0f;

/** @brief Screen terakhir yang memicu music switch */
static ScreenState _lastMusicScreen = MAIN_MENU;

/** @brief Index track yang sedang aktif (-1 = none) */
static int _activeTrackIndex = -1;

/** @brief Flag inisialisasi */
static bool _initialized = false;

/*==============================================================================
 * Track File Mapping
 *==============================================================================*/

/**
 * @brief Nama file music berdasarkan index
 *
 * Index 0: MainMenu.mp3 — MAIN_MENU
 * Index 1: DungeonMusic.mp3 — PLAY
 * Index 2: GameOver.mp3 — GAME_OVER
 * Index 3: Dungeon.mp3 — unused spare
 * Index 4: Subwoofer Lullaby.mp3 — unused spare
 */
static const char* TRACK_FILES[] = {
    "assets/audio/music/MainMenu.mp3",
    "assets/audio/music/DungeonMusic.mp3",
    "assets/audio/music/GameOver.mp3",
    "assets/audio/music/Dungeon.mp3",
    "assets/audio/music/Minecraft Volume Alpha - 3 - Subwoofer Lullaby.mp3"
};

static const int TRACK_COUNT = sizeof(TRACK_FILES) / sizeof(TRACK_FILES[0]);

/**
 * @brief Memetakan ScreenState ke index track
 * @param screen ScreenState saat ini
 * @return Index track (0-2), atau -1 jika tidak ada mapping
 */
static int ScreenToTrackIndex(ScreenState screen)
{
    switch (screen)
    {
        case MAIN_MENU: return 0;
        case PLAY:      return 1;
        case GAME_OVER: return 2;
        default:        return -1;
    }
}

/*==============================================================================
 * Lifecycle Implementation
 *==============================================================================*/

void AudioManager::Init()
{
    _masterVolume = 1.0f;
    _musicVolume  = 0.8f;
    _sfxVolume    = 1.0f;

    ::SetMasterVolume(_masterVolume);

    _lastMusicScreen = MAIN_MENU;
    _activeTrackIndex = -1;
    _initialized = true;

    TraceLog(LOG_INFO, "AUDIO: AudioManager diinisialisasi (Master=1.0, Music=0.8, SFX=1.0)");
}

void AudioManager::LoadAudioAssets()
{
    if (!_initialized)
    {
        TraceLog(LOG_WARNING, "AUDIO: LoadAudioAssets() dipanggil sebelum Init()");
        return;
    }

    // Load semua music tracks
    for (int i = 0; i < TRACK_COUNT; i++)
    {
        _tracks[i] = LoadMusicStream(TRACK_FILES[i]);
        if (_tracks[i].ctxData == nullptr)
        {
            TraceLog(LOG_WARNING, "AUDIO: Gagal muat music track: %s", TRACK_FILES[i]);
        }
        else
        {
            _tracks[i].looping = true;
            TraceLog(LOG_INFO, "AUDIO: Music track dimuat: %s", TRACK_FILES[i]);
        }
    }

    // Load SFX
    const char* sfxPath = "assets/audio/sfx/SwordSlash.mp3";
    _sfx = LoadSound(sfxPath);
    if (_sfx.stream.buffer == nullptr)
    {
        TraceLog(LOG_WARNING, "AUDIO: Gagal muat SFX: %s", sfxPath);
    }
    else
    {
        TraceLog(LOG_INFO, "AUDIO: SFX dimuat: %s", sfxPath);
    }
}

void AudioManager::Shutdown()
{
    if (!_initialized) return;

    // Stop semua music
    if (_activeTrackIndex >= 0)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
    }

    // Unload semua music tracks
    for (int i = 0; i < TRACK_COUNT; i++)
    {
        if (_tracks[i].ctxData != nullptr)
        {
            UnloadMusicStream(_tracks[i]);
            _tracks[i] = {};
        }
    }

    // Unload SFX
    UnloadSound(_sfx);
    _sfx = {};

    _activeTrackIndex = -1;
    _initialized = false;

    TraceLog(LOG_INFO, "AUDIO: AudioManager shutdown selesai");
}

/*==============================================================================
 * Per-frame Update
 *==============================================================================*/

void AudioManager::Update(ScreenState currentScreen)
{
    if (!_initialized) return;

    // Update music stream untuk track yang aktif
    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        UpdateMusicStream(_tracks[_activeTrackIndex]);

        // Terapkan volume setiap frame (supaya perubahan slider langsung terasa)
        float effectiveVolume = _musicVolume * _masterVolume;
        SetMusicVolume(_tracks[_activeTrackIndex], effectiveVolume);
    }

    // Deteksi perubahan screen untuk auto-switch
    // LOADING dan OPTIONS tidak memicu switch
    // Juga start track kalo belum ada yg playing (_activeTrackIndex == -1)
    if (currentScreen != LOADING && currentScreen != OPTIONS &&
        (_activeTrackIndex == -1 || currentScreen != _lastMusicScreen))
    {
        int newTrackIndex = ScreenToTrackIndex(currentScreen);

        if (newTrackIndex >= 0 && newTrackIndex != _activeTrackIndex)
        {
            // Stop track lama
            if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
            {
                StopMusicStream(_tracks[_activeTrackIndex]);
            }

            // Play track baru
            if (_tracks[newTrackIndex].ctxData != nullptr)
            {
                SeekMusicStream(_tracks[newTrackIndex], 0.0f);
                PlayMusicStream(_tracks[newTrackIndex]);
                _activeTrackIndex = newTrackIndex;
                TraceLog(LOG_INFO, "AUDIO: Beralih ke track %d (%s)", newTrackIndex, TRACK_FILES[newTrackIndex]);
            }
        }

        _lastMusicScreen = currentScreen;
    }
}

/*==============================================================================
 * Volume Getters (float)
 *==============================================================================*/

float AudioManager::GetMasterVolume()
{
    return _masterVolume;
}

float AudioManager::GetMusicVolume()
{
    return _musicVolume;
}

float AudioManager::GetSfxVolume()
{
    return _sfxVolume;
}

/*==============================================================================
 * Volume Setters (float, otomatis clamp)
 *==============================================================================*/

void AudioManager::SetMasterVolume(float vol)
{
    _masterVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
    ::SetMasterVolume(_masterVolume);
}

void AudioManager::SetMusicVolume(float vol)
{
    _musicVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
}

void AudioManager::SetSfxVolume(float vol)
{
    _sfxVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
}

/*==============================================================================
 * Percentage Convenience (0-100)
 *==============================================================================*/

int AudioManager::GetMasterVolumePct()
{
    return static_cast<int>(_masterVolume * 100.0f + 0.5f);
}

int AudioManager::GetMusicVolumePct()
{
    return static_cast<int>(_musicVolume * 100.0f + 0.5f);
}

int AudioManager::GetSfxVolumePct()
{
    return static_cast<int>(_sfxVolume * 100.0f + 0.5f);
}

void AudioManager::SetVolumesFromPct(int masterPct, int musicPct, int sfxPct)
{
    // Clamp ke 0-100
    if (masterPct < 0) masterPct = 0;
    if (masterPct > 100) masterPct = 100;
    if (musicPct < 0) musicPct = 0;
    if (musicPct > 100) musicPct = 100;
    if (sfxPct < 0) sfxPct = 0;
    if (sfxPct > 100) sfxPct = 100;

    SetMasterVolume(static_cast<float>(masterPct) / 100.0f);
    SetMusicVolume(static_cast<float>(musicPct) / 100.0f);
    SetSfxVolume(static_cast<float>(sfxPct) / 100.0f);
}

/*==============================================================================
 * Music Control
 *==============================================================================*/

void AudioManager::PlayTrack(const char* trackName)
{
    if (!_initialized) return;

    // Cari track berdasarkan substring nama file
    int foundIndex = -1;
    for (int i = 0; i < TRACK_COUNT; i++)
    {
        if (strstr(TRACK_FILES[i], trackName) != nullptr)
        {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex < 0 || _tracks[foundIndex].ctxData == nullptr)
    {
        TraceLog(LOG_WARNING, "AUDIO: Track '%s' tidak ditemukan atau gagal di-load", trackName);
        return;
    }

    // Stop track lama
    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
    }

    // Play track baru
    SeekMusicStream(_tracks[foundIndex], 0.0f);
    PlayMusicStream(_tracks[foundIndex]);
    _activeTrackIndex = foundIndex;

    TraceLog(LOG_INFO, "AUDIO: Memutar track: %s", TRACK_FILES[foundIndex]);
}

void AudioManager::StopMusic()
{
    if (!_initialized) return;

    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
        _activeTrackIndex = -1;
        TraceLog(LOG_INFO, "AUDIO: Music dihentikan");
    }
}

/*==============================================================================
 * SFX Control
 *==============================================================================*/

void AudioManager::PlaySfx()
{
    if (!_initialized) return;
    if (_sfx.stream.buffer == nullptr) return;

    float effectiveVolume = _sfxVolume * _masterVolume;
    SetSoundVolume(_sfx, effectiveVolume);
    PlaySound(_sfx);

    TraceLog(LOG_INFO, "AUDIO: SFX serangan diputar (volume=%.2f)", effectiveVolume);
}
