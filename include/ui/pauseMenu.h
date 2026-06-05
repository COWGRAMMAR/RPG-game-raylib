#pragma once

/**
 * @file pausemenu.h
 * @brief Pause Menu System Module
 * 
 * Handle pause menu UI pas game di-pause.
 * Muncul di tengah layar dengan tombol-tombol:
 * - Resume, Save, Load, Options, Controls, Quit
 */

#include "../lib/raylib/include/raylib.h"
#include "button.h"
#include "screen.h"
#include <array>
#include <cstdint>
#include <vector>

/*==============================================================================
 * Video Settings Constants
 *==============================================================================*/

/**
 * @struct ResOption
 * @brief Struktur untuk menyimpan resolusi yang tersedia
 * 
 * Setiap opsi berisi lebar, tinggi, dan label tampilan.
 */
struct ResOption {
    int width;
    int height;
    const char* label;
};

/*==============================================================================
 * OptionsScreen Class
 *==============================================================================*/

/**
 * @class OptionsScreen
 * @brief Class untuk menangani layar Options standalone
 * 
 * Memiliki 3 tab: Video, Audio, Keybinds.
 * Dapat diakses dari Main Menu atau Pause Menu.
 */
class OptionsScreen {
public:
    /**
     * @brief Constructor
     * 
     * Menginisialisasi semua tombol dan dimensi awal.
     */
    OptionsScreen();

    /**
     * @brief Destructor
     */
    ~OptionsScreen();

    /**
     * @brief Menampilkan layar options
     */
    void Show();

    /**
     * @brief Menyembunyikan layar options
     */
    void Hide();

    /** @brief Cek apakah layar options sedang aktif */
    bool IsActive() const;

    /**
     * @brief Memperbarui handling input
     * @param state Pointer ke GameState
     * @param mousePosition Posisi mouse saat ini
     * @param mouseClicked Status klik mouse
     */
    void Update(GameState* state, Vector2 mousePosition, bool mouseClicked);

    /**
     * @brief Me-render layar options
     * @param mousePosition Posisi mouse untuk efek hover
     */
    void Draw(Vector2 mousePosition);

    /** @brief Set layar tujuan saat tombol BACK diklik */
    void SetReturnScreen(ScreenState screen);

private:
    /**
     * @brief Menghitung dimensi dan membuat elemen UI
     */
    void CalculateDimensions();

    /// Status aktif layar options
    bool active;

    /// Flag apakah texture sudah dimuat
    bool texturesLoaded;

    /// Layar tujuan saat tombol BACK diklik
    ScreenState returnScreen;

    /// Tab yang sedang dipilih (0=Video, 1=Audio, 2=Keybinds)
    int selectedTab;

    /// Array tombol tab gambar (VIDEO, AUDIO, KEYBINDS)
    std::array<buttonImage, 3> tabButtons;

    /// Tombol BACK untuk kembali ke layar sebelumnya
    buttonImage backButton;

    /// Tombol toggle fullscreen (ON/OFF)
    buttonTxt fullscreenButton;

    /// Tombol toggle FPS display (ON/OFF)
    buttonTxt fpsButton;

    /// Tombol reset settings tab saat ini
    buttonTxt resetTabButton;

    /// Tombol reset semua settings
    buttonTxt resetOptionsButton;

    /// Status tampilan FPS (disinkronkan dengan GameState)
    bool showFPS;

    /// Lebar area options
    int width;

    /// Tinggi area options
    int height;

    /// Posisi X awal area options
    int startX;

    /// Posisi Y awal area options
    int startY;

    /// Rectangle background area options
    Rectangle backgroundRect;

    /// Texture background settings
    Texture2D bgTexture;

    /// Vektor opsi resolusi yang tersedia
    std::vector<ResOption> resolutionOptions;
};

/*==============================================================================
 * PauseMenu Class
 *==============================================================================*/

/**
 * @class PauseMenu
 * @brief Class untuk menangani pause menu
 * 
 * Menu yang muncul saat player menjeda game.
 * Di-render di atas game screen (overlay) dengan background semi-transparan.
 * 
 * Tombol-tombol yang tersedia:
 * - Resume      → melanjutkan game
 * - Save        → menyimpan state game
 * - Load        → memuat state game
 * - Options     → membuka menu options
 * - Controls    → melihat kontrol
 * - Quit        → kembali ke main menu
 */
class PauseMenu {
public:
    /**
     * @brief Constructor
     * 
     * Menginisialisasi menu dan semua tombol.
     */
    PauseMenu();
    
    /**
     * @brief Destructor
     */
    ~PauseMenu();

    /**
     * @brief Menampilkan pause menu
     * @note Set active = true, game logic harus berhenti
     */
    void Show();
    
    /**
     * @brief Menyembunyikan pause menu
     * @note Set active = false, game logic lanjut lagi
     */
    void Hide();
    
    /**
     * @brief Memeriksa apakah pause menu sedang aktif
     * @return true jika menu muncul, false jika tidak
     */
    bool IsActive() const;

    /**
     * @brief Memperbarui logic pause menu
     * @param state Pointer ke GameState
     * @param mousePosition Posisi mouse saat ini
     * @param mouseClicked Apakah tombol mouse ditekan
     */
    void Update(GameState* state, Vector2 mousePosition, bool mouseClicked);
    
    /**
     * @brief Me-render pause menu ke layar
     * @param mousePosition Posisi mouse saat ini untuk efek hover
     */
    void Draw(Vector2 mousePosition);

private:
    /**
     * @brief Menghitung dimensi menu berdasarkan layar
     */
    void CalculateDimensions();
    
    /**
     * @brief Handle klik pada tombol berdasarkan index
     * @param buttonIndex Index tombol yang diklik (0-5)
     * @param state Pointer ke GameState
     */
    void HandleButtonClick(int buttonIndex, GameState* state);

    /**
     * @brief Memuat texture button dari disk (lazy, sekali saja)
     */
    void LoadTextures();

    /*==========================================================================
     * Private Members
     *==========================================================================*/
    
    /// Status aktif menu
    bool active;

    /// Flag apakah texture sudah dimuat
    bool texturesLoaded;

    /// Array tombol gambar (6 buah)
    std::array<buttonImage, 6> buttons;

    /// Texture background panel
    Texture2D bgTexture;

    /// Posisi background panel di layar
    Vector2 position;

    /// Lebar background panel
    int width;

    /// Tinggi background panel
    int height;

    /// Rectangle untuk background panel
    Rectangle backgroundRect;
};
