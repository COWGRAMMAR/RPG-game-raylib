#pragma once

/**
 * @file saveLoadScreen.h
 * @brief Menu Simpan/Muat Game Module
 *
 * Header ini mendeklarasikan kelas SaveLoadScreen untuk menangani
 * UI menu simpan dan muat game. Mengikuti pola yang sama dengan
 * OptionsScreen.
 */

#include "../lib/raylib/include/raylib.h"
#include "button.h"
#include "screen.h"
#include "popup.h"
#include <string>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

/// Mode operasi SaveLoadScreen
enum class SaveLoadMode {
    SAVE_MODE,  ///< Mode menyimpan game (pilih slot)
    LOAD_MODE   ///< Mode memuat game (pilih slot)
};

/*==============================================================================
 * SaveLoadScreen Class
 *==============================================================================*/

/**
 * @class SaveLoadScreen
 * @brief Class untuk menangani layar Save/Load
 *
 * Menyediakan antarmuka untuk menyimpan dan memuat state game.
 * Di-render di atas game screen dengan latar belakang gelap.
 * Menggunakan tombol berbasis teks untuk navigasi.
 */
class SaveLoadScreen {
public:
    /**
     * @brief Constructor
     *
     * Menginisialisasi semua member dan tombol navigasi.
     */
    SaveLoadScreen();

    /**
     * @brief Destructor
     */
    ~SaveLoadScreen();

    /**
     * @brief Menampilkan layar save/load
     */
    void Show();

    /**
     * @brief Menyembunyikan layar save/load
     */
    void Hide();

    /** @brief Cek apakah layar save/load sedang aktif */
    bool IsActive() const;

    /**
     * @brief Memperbarui handling input
     * @param state Pointer ke GameState
     * @param mousePosition Posisi mouse saat ini
     * @param mouseClicked Status klik mouse
     */
    void Update(GameState* state, Vector2 mousePosition, bool mouseClicked);

    /**
     * @brief Me-render layar save/load
     * @param mousePosition Posisi mouse untuk efek hover
     */
    void Draw(Vector2 mousePosition);

    /** @brief Set layar tujuan saat tombol BACK diklik */
    void SetReturnScreen(ScreenState screen);

    /** @brief Set mode operasi (save/load) */
    void SetMode(SaveLoadMode mode);

private:
    /**
     * @brief Menghitung dimensi dan posisi elemen UI
     */
    void CalculateDimensions();

    /**
     * @brief Dapatkan index slot berdasarkan posisi klik
     * @param mousePosition Posisi mouse
     * @return Index slot (0-9) atau -1 jika tidak ada slot di posisi tersebut
     */
    int GetSlotAtPosition(Vector2 mousePosition);

    /**
     * @brief Gambar satu slot box
     * @param slotIndex Indeks slot (0-9)
     * @param posX Posisi X slot
     * @param posY Posisi Y slot
     * @param occupied Apakah slot terisi data
     * @param mapName Nama map yang ditampilkan
     * @param timestamp Timestamp save
     * @param mousePosition Posisi mouse untuk efek hover
     * @param enabled Apakah slot dapat diinteraksi
     */
    void DrawSlotBox(int slotIndex, int posX, int posY, bool occupied, const std::string& mapName, const std::string& timestamp, Vector2 mousePosition, bool enabled);

    /**
     * @brief Gambar grid slot manual dan autosave
     * @param mousePosition Posisi mouse untuk efek hover
     */
    void DrawSlotGrid(Vector2 mousePosition);

    /**
     * @brief Muat metadata semua slot dari disk
     *
     * Untuk setiap slot N (0-9), periksa saves/slot_N/manual/manual.json.
     * Jika ada, baca mapDisplayName dan timestamp.
     * Jika tidak, tandai sebagai kosong.
     */
    void RefreshSlotMetadata(void);

    /// Jumlah slot manual (0-4)
    static constexpr int MANUAL_SLOT_COUNT = 6;
    /// Jumlah slot autosave (0-4)
    static constexpr int AUTOSAVE_SLOT_COUNT = 6;
    /// Slot box width in pixels
    static constexpr int SLOT_WIDTH = 250;
    /// Slot box height in pixels
    static constexpr int SLOT_HEIGHT = 70;
    /// Gap between slot boxes
    static constexpr int SLOT_GAP = 10;

    /// Status aktif layar save/load
    bool active;

    /// Flag apakah texture sudah dimuat
    bool texturesLoaded;

    /// Layar tujuan saat tombol BACK diklik
    ScreenState returnScreen;

    /// Tombol BACK untuk kembali ke layar sebelumnya
    buttonTxt backButton;

    /// Lebar area save/load
    int width;

    /// Tinggi area save/load
    int height;

    /// Posisi X awal area save/load
    int startX;

    /// Posisi Y awal area save/load
    int startY;

    /// Rectangle background area save/load
    Rectangle backgroundRect;

    /// Texture background
    Texture2D bgTexture;

    /// Array status okupansi slot (true=terisi)
    bool slotOccupied[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];

    /// Array nama map per slot
    std::string slotMapName[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];

    /// Array timestamp per slot
    std::string slotTimestamp[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];

    /// Mode operasi saat ini (save/load)
    SaveLoadMode m_mode;
    Popup m_overwritePopup;   ///< Popup konfirmasi timpa save
    Popup m_loadPopup;        ///< Popup konfirmasi load game
    bool m_showOverwritePopup; ///< Flag tampilkan overwrite popup
    bool m_showLoadPopup;      ///< Flag tampilkan load popup
    int m_selectedSlot;        ///< Slot yang dipilih untuk save/load
};
