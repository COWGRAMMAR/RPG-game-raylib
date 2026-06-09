#pragma once

/**
 * @file screen.h
 * @brief Screen & GameState Management Module
 *
 * Header ini mendeklarasikan sistem virtual screen, scaling window,
 * dan state utama game untuk proses update serta rendering.
 */

#include "../lib/raylib/include/raylib.h"
#include <atomic>
#include <cstdint>
#include <string>

namespace Time
{
    /** @brief Timestep tetap untuk update game, setara 60 FPS */
    inline constexpr float DELTA_TIME = 1.0f / 60.0f;
    /** @brief Batas maksimum durasi frame agar update tidak meloncat terlalu jauh */
    inline constexpr float MAX_FRAME = 0.25f;
}

/*==============================================================================
 * Virtual Screen Constants
 *==============================================================================*/

/** @brief Ukuran layar virtual yang dipakai seluruh proses rendering */
extern const int GameScreenWidth;
extern const int GameScreenHeight;

/*==============================================================================
 * ScreenState Enum
 *==============================================================================*/

/**
 * @brief Daftar state utama game
 */
enum ScreenState : std::uint8_t
{
    MAIN_MENU,  // State menu utama
    LOADING,    // State loading aset
    PLAY,       // State gameplay aktif
    OPTIONS,    // State menu pengaturan
    GAME_OVER   // State game over
};

/*==============================================================================
 * GameState Struct
 *==============================================================================*/

/**
 * @brief Menyimpan data utama rendering dan state game
 */
struct GameState
{
    RenderTexture2D Dungeon;    // Render texture virtual (1280x720) — target rendering semua game
    float ScaleMultiplier;      // Rasio scale layar virtual ke window asli (dihitung tiap frame)
    int WindowScreenWidth;      // Ukuran window asli saat ini (bisa berubah kalo resize)
    int WindowScreenHeight;     // Ukuran window asli saat ini (bisa berubah kalo resize)
    ScreenState currentScreen;  // State game yang aktif (MAIN_MENU / PLAY / OPTIONS)
    ScreenState previousScreen; // Screen sebelum OPTIONS — buat return button
    bool showFPS;               // Tampilkan FPS counter di HUD

    /* Loading State Variables */
    float loadingProgress;   // Progress loading saat ini (0–100)
    const char *loadingText; // Teks status loading yang ditampilkan
    bool loadingComplete;    // Flag menandakan loading sudah selesai
    int loadingStage;        // Stage loading saat ini (reset setiap masuk LOADING state)
    bool assetsLoaded;       // Flag menandakan asset sudah dimuat ke memori
    bool enteredLoading;     // Flag menandakan sudah pernah masuk LOADING state

    /* Map Switch State Variables */
    bool isSwitchingMap;         // Flag menandakan sedang dalam proses switch map
    bool isGoingBack;            // Flag menandakan sedang dalam proses kembali ke map sebelumnya
    std::string pendingMapPath;  // Path map tujuan yang akan dimuat
    std::string pendingDoorName; // Nama door atau spawn point tujuan

    /* Revive State Variables */
    Vector2 startSpawnPos{};     // Posisi spawn start room untuk revive
};

/** @brief Pointer global ke GameState aktif */
extern GameState *gState;

/*==============================================================================
 * Screen Functions
 *==============================================================================*/

/**
 * @brief Inisialisasi window, audio, dan virtual screen
 * @return GameState yang sudah siap dipakai
 */
GameState InitScreen(void);

/**
 * @brief Update ukuran window dan nilai scale multiplier
 * @param state Pointer ke GameState aktif
 */
void UpdateGame(GameState *state);

/** @brief Inisialisasi entity dan camera di awal game */
void InitAll(void);

/**
 * @brief Render seluruh isi game ke virtual screen
 * @param state Pointer ke GameState aktif
 */
void DrawRenderTexture(GameState *state);

/**
 * @brief Render elemen UI di atas virtual screen
 * @param state Pointer ke GameState aktif
 */
void DrawUIOverlay(GameState *state);

/** @brief Draw player HUD */
void DrawPlayerHUD();

/**
 * @brief Konversi posisi mouse dari window space ke virtual screen space
 * @param state Pointer ke GameState aktif
 * @return Posisi mouse dalam koordinat virtual screen
 */
Vector2 GetVirtualMousePosition(GameState *state);

/** @brief Jalankan seluruh update logic game per frame */
void UpdateLogicAll(void);

/**
 * @brief Gambar virtual screen ke window asli dengan scaling
 * @param state Pointer ke GameState aktif
 */
void DrawRenderWindows(GameState *state);

/**
 * @brief Bersihkan resource sebelum game ditutup
 * @param state Pointer ke GameState aktif
 */
void GameShutDown(GameState *state);

/*==============================================================================
 * Window & Video Settings Functions
 *==============================================================================*/

/** @brief Toggle antara fullscreen dan windowed mode */
void ToggleFullscreenMode(void);

/**
 * @brief Set ukuran window ke resolusi tertentu
 * @param width Lebar window baru
 * @param height Tinggi window baru
 */
void SetResolution(int width, int height);

/**
 * @brief Ambil resolusi saat ini
 * @return Rectangle berisi width dan height
 */
Rectangle GetCurrentResolution(void);

/**
 * @brief Gambar background gradient yang seragam untuk semua layar non-gameplay
 * (main menu, loading screen, dll). Nantinya bisa diganti dengan animated BG.
 */
void DrawMenuBackground(void);

/**
 * @brief Ambil resolusi monitor utama
 * @return Rectangle berisi width dan height monitor
 */
Rectangle GetMonitorResolution(void);

/**
 * @brief Cek apakah sedang dalam mode fullscreen
 * @return true kalo fullscreen
 */
bool IsFullscreen(void);

/** @brief Jumlah enemy per spawn point (sementara) */
constexpr int ENEMY_SPAWN_COUNT = 10;
