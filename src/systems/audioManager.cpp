/**
 * @file audioManager.cpp
 * @brief Implementasi Audio Manager Module
 *
 * Mengelola volume (Master, Music, SFX) menggunakan raylib audio API.
 * Memuat 5 music tracks dari assets/audio/music/ dan 1 SFX.
 * Music playback otomatis berganti sesuai ScreenState.
 */

#include "systems/audioManager.h"
#include "raylib.h"
#include <cstring>
#include <unordered_map>

/** @brief Array music tracks yang sudah di-load */
static Music _tracks[7] = {};

static float _masterVolume = 1.0f;
static float _musicVolume = 0.8f;
static float _sfxVolume = 1.0f;
static float _videoVolume = 1.0f;

/** @brief Screen terakhir yang memicu music switch */
static ScreenState _lastMusicScreen = MAIN_MENU;

/** @brief Index track yang sedang aktif (-1 = none) */
static int _activeTrackIndex = -1;

/** @brief Index track boss music */
static const int BOSS_TRACK_INDEX = 5;

/** @brief Flag inisialisasi */
static bool _initialized = false;

/** @brief Blok auto-switch biar WinTheme gak di-overwrite pas VICTORY phase */
static bool _blockAutoSwitch = false;

/**
 * @brief Nama file music berdasarkan index
 *
 * Index 0: MainMenu-Startup.mp3 — MAIN_MENU (startup piece, non-looping, auto-transitions to 3)
 * Index 1: DungeonMusic.mp3 — PLAY
 * Index 2: GameOver.mp3 — GAME_OVER
 * Index 3: MainMenu-Loop.mp3 — MAIN_MENU (looping, plays after startup finishes)
 * Index 4: Subwoofer Lullaby.mp3 — unused spare
 * Index 5: BossMusic.mp3 — turn-based boss fight
 * Index 6: WinTheme.mp3 — turn-based victory
 */
static const char *TRACK_FILES[] = {
    "assets/audio/music/MainMenu-Startup.mp3",
    "assets/audio/music/DungeonMusic.mp3",
    "assets/audio/music/GameOver.mp3",
    "assets/audio/music/MainMenu-Loop.mp3",
    "assets/audio/music/Minecraft Volume Alpha - 3 - Subwoofer Lullaby.mp3",
    "assets/audio/music/BossMusic.mp3",
    "assets/audio/music/WinTheme.mp3"};
static const int MAINMENU_LOOP_INDEX = 3;
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
    case MAIN_MENU:
        return 0;
    case PLAY:
        return 1;
    case GAME_OVER:
        return 2;
    default:
        return -1;
    }
}

/** @name Lifecycle */
/**@{*/

void AudioManager::Init()
{
    _masterVolume = 1.0f;
    _musicVolume = 1.0f;
    _sfxVolume = 1.0f;
    _videoVolume = 1.0f;

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

    if (_tracks[0].ctxData != nullptr)
        _tracks[0].looping = false;
}

void AudioManager::Shutdown()
{
    if (!_initialized)
        return;

    if (_activeTrackIndex >= 0)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
    }

    for (int i = 0; i < TRACK_COUNT; i++)
    {
        if (_tracks[i].ctxData != nullptr)
        {
            UnloadMusicStream(_tracks[i]);
            _tracks[i] = {};
        }
    }

    _activeTrackIndex = -1;
    _initialized = false;

    TraceLog(LOG_INFO, "AUDIO: AudioManager shutdown selesai");
}

void AudioManager::UnloadMusic()
{
    if (!_initialized)
        return;

    if (_activeTrackIndex >= 0)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
    }

    for (int i = 0; i < TRACK_COUNT; i++)
    {
        if (_tracks[i].ctxData != nullptr)
        {
            UnloadMusicStream(_tracks[i]);
            _tracks[i] = {};
        }
    }

    _activeTrackIndex = -1;

    TraceLog(LOG_INFO, "AUDIO: Music tracks unloaded");
}

/**@}*/

void AudioManager::Update(ScreenState currentScreen)
{
    if (!_initialized)
        return;

    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        UpdateMusicStream(_tracks[_activeTrackIndex]);

        float effectiveVolume = _musicVolume * _masterVolume;
        SetMusicVolume(_tracks[_activeTrackIndex], effectiveVolume);
    }

    // _blockAutoSwitch: skip auto-switch (VICTORY phase, biar WinTheme gak di-overwrite)
    if (!_blockAutoSwitch && currentScreen != LOADING && currentScreen != OPTIONS &&
        (_activeTrackIndex == -1 || currentScreen != _lastMusicScreen))
    {
        int newTrackIndex = ScreenToTrackIndex(currentScreen);

        if (newTrackIndex >= 0 && newTrackIndex != _activeTrackIndex)
        {
            if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
                StopMusicStream(_tracks[_activeTrackIndex]);

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

    if (_activeTrackIndex == 0 && _tracks[0].ctxData != nullptr)
    {
        float played = GetMusicTimePlayed(_tracks[0]);
        float length = GetMusicTimeLength(_tracks[0]);
        if (length > 0.0f && played >= length - 0.1f)
        {
            StopMusicStream(_tracks[0]);

            if (_tracks[MAINMENU_LOOP_INDEX].ctxData != nullptr)
            {
                SeekMusicStream(_tracks[MAINMENU_LOOP_INDEX], 0.0f);
                PlayMusicStream(_tracks[MAINMENU_LOOP_INDEX]);
                _activeTrackIndex = MAINMENU_LOOP_INDEX;
                TraceLog(LOG_INFO, "AUDIO: MainMenu startup selesai, beralih ke loop track");
            }
        }
    }
}

/** @name Volume */
/**@{*/

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

float AudioManager::GetVideoVolume()
{
    return _videoVolume;
}

void AudioManager::SetMasterVolume(float vol)
{
    _masterVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                       : vol;
    ::SetMasterVolume(_masterVolume);
}

void AudioManager::SetMusicVolume(float vol)
{
    _musicVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                      : vol;
}

void AudioManager::SetSfxVolume(float vol)
{
    _sfxVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                    : vol;
}

void AudioManager::SetVideoVolume(float vol)
{
    _videoVolume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                      : vol;
}

int AudioManager::GetMasterVolumePct()
{
    return static_cast<int>(_masterVolume * VOLUME_PCT_MAX_F + VOLUME_ROUND);
}

int AudioManager::GetMusicVolumePct()
{
    return static_cast<int>(_musicVolume * VOLUME_PCT_MAX_F + VOLUME_ROUND);
}

int AudioManager::GetSfxVolumePct()
{
    return static_cast<int>(_sfxVolume * VOLUME_PCT_MAX_F + VOLUME_ROUND);
}

int AudioManager::GetVideoVolumePct()
{
    return static_cast<int>(_videoVolume * VOLUME_PCT_MAX_F + VOLUME_ROUND);
}

void AudioManager::SetVolumesFromPct(int masterPct, int musicPct, int sfxPct, int videoPct)
{
    if (masterPct < 0)
        masterPct = 0;
    if (masterPct > VOLUME_PCT_MAX)
        masterPct = VOLUME_PCT_MAX;
    if (musicPct < 0)
        musicPct = 0;
    if (musicPct > VOLUME_PCT_MAX)
        musicPct = VOLUME_PCT_MAX;
    if (sfxPct < 0)
        sfxPct = 0;
    if (sfxPct > VOLUME_PCT_MAX)
        sfxPct = VOLUME_PCT_MAX;
    if (videoPct < 0)
        videoPct = 0;
    if (videoPct > VOLUME_PCT_MAX)
        videoPct = VOLUME_PCT_MAX;

    SetMasterVolume(static_cast<float>(masterPct) / VOLUME_PCT_MAX_F);
    SetMusicVolume(static_cast<float>(musicPct) / VOLUME_PCT_MAX_F);
    SetSfxVolume(static_cast<float>(sfxPct) / VOLUME_PCT_MAX_F);
    SetVideoVolume(static_cast<float>(videoPct) / VOLUME_PCT_MAX_F);
}

/**@}*/

/** @name Music Control */
/**@{*/

void AudioManager::PlayTrack(const char *trackName)
{
    if (!_initialized)
        return;

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

    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
    }

    SeekMusicStream(_tracks[foundIndex], 0.0f);
    PlayMusicStream(_tracks[foundIndex]);
    _activeTrackIndex = foundIndex;

    TraceLog(LOG_INFO, "AUDIO: Memutar track: %s", TRACK_FILES[foundIndex]);
}

void AudioManager::StopMusic()
{
    if (!_initialized)
        return;

    if (_activeTrackIndex >= 0 && _tracks[_activeTrackIndex].ctxData != nullptr)
    {
        StopMusicStream(_tracks[_activeTrackIndex]);
        _activeTrackIndex = -1;
        TraceLog(LOG_INFO, "AUDIO: Music dihentikan");
    }
}

void AudioManager::ResetToScreenTrack()
{
    if (!_initialized)
        return;
    _lastMusicScreen = MAIN_MENU;
    TraceLog(LOG_INFO, "AUDIO: Reset track untuk auto-switch");
}

/**@}*/

void AudioManager::BlockAutoSwitch()
{
    _blockAutoSwitch = true;
}

void AudioManager::UnblockAutoSwitch()
{
    _blockAutoSwitch = false;
}

/** @name SFX */
/**@{*/

struct SoundPool
{
    Sound sounds[4];
    int currentIndex;
};

static std::unordered_map<std::string, SoundPool> loadedSFX;

static void LoadSFXToPool(const std::string &name, const char *path)
{
    if (!IsAudioDeviceReady())
        return;

    Sound original = LoadSound(path);
    if (original.frameCount == 0)
    {
        TraceLog(LOG_WARNING, "SFX: Failed to load %s, alias creation skipped.", path);
        return;
    }

    SoundPool pool;
    pool.sounds[0] = original;
    for (int i = 1; i < 4; i++)
    {
        pool.sounds[i] = LoadSoundAlias(original);
    }
    pool.currentIndex = 0;
    loadedSFX[name] = pool;
}

void AudioManager::InitSFX()
{
    // Unload dulu kalo ada sisa dari game sebelumnya (safety buat future restart)
    CloseSFX();

    LoadSFXToPool("thrust", "assets/audio/sfx/thrust.mp3");
    LoadSFXToPool("arrow", "assets/audio/sfx/arrow.mp3");
    LoadSFXToPool("attack", "assets/audio/sfx/attack.mp3");
    LoadSFXToPool("chest", "assets/audio/sfx/chest.mp3");
    LoadSFXToPool("crate", "assets/audio/sfx/crate.mp3");
    LoadSFXToPool("dash", "assets/audio/sfx/dash.mp3");
    LoadSFXToPool("explosion", "assets/audio/sfx/explosion.mp3");
    LoadSFXToPool("hurt", "assets/audio/sfx/hurt.mp3");
    LoadSFXToPool("inventori", "assets/audio/sfx/inventori.mp3");
    LoadSFXToPool("pickup-item", "assets/audio/sfx/pickup-item.mp3");
    LoadSFXToPool("rifle", "assets/audio/sfx/rifle.mp3");
    LoadSFXToPool("slam", "assets/audio/sfx/slam.mp3");
    LoadSFXToPool("slash-mid", "assets/audio/sfx/slash-mid.mp3");
    LoadSFXToPool("slash-short", "assets/audio/sfx/slash-short.mp3");
    LoadSFXToPool("walk", "assets/audio/sfx/walk.mp3");

    // Pre-warm audio to prevent first-play lag on Windows
    for (auto &pair : loadedSFX)
    {
        SetSoundVolume(pair.second.sounds[0], 0.0f);
        PlaySound(pair.second.sounds[0]);
    }

    TraceLog(LOG_INFO, "SFX: Successfully loaded and pre-warmed all sound effects");
}

void AudioManager::CloseSFX()
{
    if (!IsAudioDeviceReady())
        return;

    for (auto &pair : loadedSFX)
    {
        for (int i = 1; i < 4; i++)
        {
            UnloadSoundAlias(pair.second.sounds[i]);
        }
        UnloadSound(pair.second.sounds[0]);
    }
    loadedSFX.clear();
    TraceLog(LOG_INFO, "SFX: Successfully unloaded all sound effects");
}

void AudioManager::PlaySFX(const std::string &name)
{
    if (!IsAudioDeviceReady())
        return;

    auto it = loadedSFX.find(name);
    if (it != loadedSFX.end())
    {
        float effectiveVolume = _sfxVolume * _masterVolume;
        SetSoundVolume(it->second.sounds[it->second.currentIndex], effectiveVolume);
        PlaySound(it->second.sounds[it->second.currentIndex]);
        it->second.currentIndex = (it->second.currentIndex + 1) % 4;
    }
    else
    {
        TraceLog(LOG_WARNING, "SFX: Sound not found: %s", name.c_str());
    }
}

/**@}*/
