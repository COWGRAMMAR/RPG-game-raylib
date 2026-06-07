/**
 * @file saveLoadScreen.cpp
 * @brief Menu Simpan/Muat Game Module
 *
 * Implementasi kelas SaveLoadScreen untuk menangani
 * UI menu simpan dan muat game.
 */

#include "../../include/ui/saveLoadScreen.h"
#include "../../include/core/game_state_saver.h"
#include "fonts.h"
#include "../lib/json/include/nlohmann/json.hpp"

/*==============================================================================
 * Constructor / Destructor
 *==============================================================================*/

/**
 * @brief Constructor
 *
 * Menginisialisasi semua member dan tombol navigasi.
 * Menggunakan buttonTxt (berbasis teks) untuk tombol BACK.
 */
SaveLoadScreen::SaveLoadScreen()
    : active(false)
    , texturesLoaded(false)
    , returnScreen(PLAY)
    , width(0)
    , height(0)
    , startX(0)
    , startY(0)
    , bgTexture({0})
    , slotOccupied{}
    , slotMapName{}
    , slotTimestamp{}
    , m_mode(SaveLoadMode::SAVE_MODE)
    , m_overwritePopup("Overwrite existing save?", "Overwrite", "Cancel", 0.7f)
    , m_loadPopup("Load this save?", "Load", "Cancel", 0.7f)
    , m_showOverwritePopup(false)
    , m_showLoadPopup(false)
    , m_selectedSlot(-1)
{
}

/**
 * @brief Destructor
 *
 * Membersihkan resource texture background jika sudah dimuat.
 */
SaveLoadScreen::~SaveLoadScreen()
{
    if (bgTexture.id != 0) {
        UnloadTexture(bgTexture);
    }
}

/*==============================================================================
 * Public Methods
 *==============================================================================*/

/**
 * @brief Menampilkan layar save/load
 *
 * Mengaktifkan flag active dan menghitung ulang dimensi UI.
 * Texture background akan dimuat saat Show() pertama kali dipanggil.
 */
void SaveLoadScreen::Show()
{
    active = true;
    if (!texturesLoaded) {
        // Texture akan dimuat di task mendatang jika diperlukan
        texturesLoaded = true;
    }
    CalculateDimensions();
    RefreshSlotMetadata();
}

/**
 * @brief Menyembunyikan layar save/load
 */
void SaveLoadScreen::Hide()
{
    active = false;
}

/**
 * @brief Memeriksa apakah layar save/load sedang aktif
 * @return true jika aktif, false jika tidak
 */
bool SaveLoadScreen::IsActive() const
{
    return active;
}

/**
 * @brief Mengatur layar kembali saat BACK diklik
 * @param screen Layar tujuan
 */
void SaveLoadScreen::SetReturnScreen(ScreenState screen)
{
    returnScreen = screen;
}

/**
 * @brief Set mode operasi save/load
 * @param mode Mode operasi (SAVE_MODE atau LOAD_MODE)
 */
void SaveLoadScreen::SetMode(SaveLoadMode mode)
{
    m_mode = mode;
}

/*==============================================================================
 * Update & Draw
 *==============================================================================*/

/**
 * @brief Memperbarui handling input
 * @param state Pointer ke GameState
 * @param mousePosition Posisi mouse saat ini
 * @param mouseClicked Status klik mouse
 *
 * Menangani popup konfirmasi, klik slot save/load,
 * dan tombol BACK.
 */
void SaveLoadScreen::Update(GameState* state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active) {
        return;
    }

    // Handle overwrite popup
    if (m_showOverwritePopup) {
        m_overwritePopup.Update(mousePosition, mouseClicked);
        if (m_overwritePopup.IsConfirmClicked()) {
            m_showOverwritePopup = false;
            SetActiveSlot(m_selectedSlot);
            SaveGameState(state);
            WriteSaveFile(GetSlotPath(m_selectedSlot, "manual"));
            active = false;
            state->currentScreen = returnScreen;
        } else if (!m_overwritePopup.IsActive()) {
            m_showOverwritePopup = false; // Cancelled
        }
        return;
    }

    // Handle load popup
    if (m_showLoadPopup) {
        m_loadPopup.Update(mousePosition, mouseClicked);
        if (m_loadPopup.IsConfirmClicked()) {
            m_showLoadPopup = false;
            SetActiveSlot(m_selectedSlot);
            {
                std::string path = GetSlotPath(m_selectedSlot, "manual");
                if (ReadSaveFile(path)) {
                    RestoreGameState(state);
                }
            }
            active = false;
            state->currentScreen = LOADING;
        } else if (!m_loadPopup.IsActive()) {
            m_showLoadPopup = false; // Cancelled
        }
        return;
    }

    // Handle slot clicks
    if (mouseClicked) {
        int clickedSlot = GetSlotAtPosition(mousePosition);
        if (clickedSlot >= 0) {
            if (m_mode == SaveLoadMode::SAVE_MODE) {
                // Autosave slots disabled in save mode
                if (clickedSlot >= MANUAL_SLOT_COUNT) {
                    return;
                }

                m_selectedSlot = clickedSlot;
                if (slotOccupied[clickedSlot]) {
                    m_overwritePopup.Show();
                    m_showOverwritePopup = true;
                } else {
                    SetActiveSlot(clickedSlot);
                    SaveGameState(state);
                    WriteSaveFile(GetSlotPath(clickedSlot, "manual"));
                    active = false;
                    state->currentScreen = returnScreen;
                }
            } else if (m_mode == SaveLoadMode::LOAD_MODE) {
                // Empty slots disabled in load mode
                if (!slotOccupied[clickedSlot]) {
                    return;
                }

                m_selectedSlot = clickedSlot;
                m_loadPopup.Show();
                m_showLoadPopup = true;
            }
            return; // Slot clicked, don't process backButton
        }
    }

    // Back button
    if (backButton.isClicked(mousePosition, mouseClicked)) {
        active = false;
        state->currentScreen = returnScreen;
    }
}

/**
 * @brief Me-render layar save/load
 * @param mousePosition Posisi mouse untuk efek hover
 *
 * Menggambar background menu, judul sesuai mode (SAVE/LOAD),
 * grid slot manual dan autosave, tombol BACK, serta popup
 * konfirmasi jika aktif.
 */
void SaveLoadScreen::Draw(Vector2 mousePosition)
{
    if (!active) {
        return;
    }

    DrawMenuBackground();

    if (bgTexture.id != 0) {
        DrawTexture(bgTexture, startX, startY, WHITE);
    } else {
        Color bgColor = {40, 40, 40, 230};
        DrawRectangleRec(backgroundRect, bgColor);
        DrawRectangleLinesEx(backgroundRect, 2, WHITE);
    }

    // Draw header based on mode
    const char* headerText = (m_mode == SaveLoadMode::SAVE_MODE) ? "SAVE GAME" : "LOAD GAME";
    int headerFontSize = 28;
    Vector2 headerTextSize = MeasureTextEx(fontLoadingTitle, headerText, headerFontSize, 1);
    int headerX = startX + (int)(width - headerTextSize.x) / 2;
    DrawTextEx(fontLoadingTitle, headerText, Vector2{(float)headerX, (float)(startY + 18)}, headerFontSize, 1, WHITE);

    // Draw slot grid
    DrawSlotGrid(mousePosition);

    backButton.Draw(mousePosition);

    // Draw popups on top
    if (m_showOverwritePopup) {
        m_overwritePopup.Draw(mousePosition);
    }
    if (m_showLoadPopup) {
        m_loadPopup.Draw(mousePosition);
    }
}

/*==============================================================================
 * Private Methods
 *==============================================================================*/

/**
 * @brief Menghitung dimensi dan posisi elemen UI
 *
 * Mengatur ukuran panel (600x400), memusatkannya di layar,
 * dan memposisikan tombol BACK di pojok kanan bawah panel.
 */
void SaveLoadScreen::CalculateDimensions()
{
    width = 850;
    height = 500;
    startX = (GameScreenWidth - width) / 2;
    startY = (GameScreenHeight - height) / 2;

    backgroundRect = {
        static_cast<float>(startX),
        static_cast<float>(startY),
        static_cast<float>(width),
        static_cast<float>(height)
    };

    backButton = buttonTxt(
        "BACK",
        startX + width - 100,
        startY + height - 50,
        24,
        WHITE,
        0.7F);
}

/**
 * @brief Dapatkan index slot berdasarkan posisi klik
 * @param mousePosition Posisi mouse
 * @return Index slot (0-9) atau -1 jika tidak ada slot di posisi tersebut
 */
int SaveLoadScreen::GetSlotAtPosition(Vector2 mousePosition)
{
    int manualRow1Y = startY + 75;
    int rowWidth3 = 3 * SLOT_WIDTH + 2 * SLOT_GAP;
    int row1X = startX + (width - rowWidth3) / 2;

    // Manual row 1 (slots 0, 1, 2)
    for (int i = 0; i < 3; i++) {
        int slotX = row1X + i * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(manualRow1Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect)) return i;
    }

    int manualRow2Y = manualRow1Y + SLOT_HEIGHT + SLOT_GAP;

    // Manual row 2 (slots 3, 4, 5)
    for (int i = 3; i < 6; i++) {
        int slotX = row1X + (i - 3) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(manualRow2Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect)) return i;
    }

    int autoLabelY = manualRow2Y + SLOT_HEIGHT + 15;
    int autoRow1Y = autoLabelY + 25;

    // Auto row 1 (slots 6, 7, 8)
    for (int i = 6; i < 9; i++) {
        int slotX = row1X + (i - 6) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(autoRow1Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect)) return i;
    }

    int autoRow2Y = autoRow1Y + SLOT_HEIGHT + SLOT_GAP;

    // Auto row 2 (slots 9, 10, 11)
    for (int i = 9; i < 12; i++) {
        int slotX = row1X + (i - 9) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(autoRow2Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect)) return i;
    }

    return -1;
}

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
void SaveLoadScreen::DrawSlotBox(int slotIndex, int posX, int posY, bool occupied, const std::string& mapName, const std::string& timestamp, Vector2 mousePosition, bool enabled)
{
    Rectangle slotRect = {
        static_cast<float>(posX),
        static_cast<float>(posY),
        static_cast<float>(SLOT_WIDTH),
        static_cast<float>(SLOT_HEIGHT)
    };

    bool hovered = enabled && CheckCollisionPointRec(mousePosition, slotRect);

    Color bgColor;
    if (!enabled) {
        bgColor = {20, 20, 30, 140};
    } else if (hovered) {
        bgColor = {70, 70, 100, 220};
    } else if (occupied) {
        bgColor = {50, 50, 70, 220};
    } else {
        bgColor = {30, 30, 40, 180};
    }

    DrawRectangleRec(slotRect, bgColor);

    Color borderColor;
    if (!enabled) {
        borderColor = {60, 60, 70, 100};
    } else {
        borderColor = occupied ? (hovered ? WHITE : (Color){180, 180, 200, 255}) : GRAY;
    }
    DrawRectangleLinesEx(slotRect, 1, borderColor);

    if (!enabled && slotIndex >= MANUAL_SLOT_COUNT && m_mode == SaveLoadMode::SAVE_MODE) {
        DrawTextEx(fontKeybindEntry, "Auto Save", Vector2{(float)(posX + 5), (float)(posY + 5)}, 16, 1, DARKGRAY);
    } else {
        DrawTextEx(fontKeybindEntry, TextFormat("Slot %d", slotIndex), Vector2{(float)(posX + 5), (float)(posY + 5)}, 16, 1, enabled ? LIGHTGRAY : DARKGRAY);
    }

    if (occupied) {
        DrawTextEx(fontKeybindEntry, mapName.c_str(), Vector2{(float)(posX + 5), (float)(posY + 24)}, 18, 1, enabled ? WHITE : GRAY);
        DrawTextEx(fontKeybindEntry, timestamp.c_str(), Vector2{(float)(posX + 5), (float)(posY + 48)}, 14, 1, enabled ? (Color){180, 180, 180, 255} : (Color){80, 80, 80, 255});
    } else {
        const char* emptyText = "Empty";
        Vector2 emptyTextSize = MeasureTextEx(fontKeybindEntry, emptyText, 20, 1);
        int emptyX = posX + (SLOT_WIDTH - (int)emptyTextSize.x) / 2;
        int emptyY = posY + (SLOT_HEIGHT - 20) / 2;
        DrawTextEx(fontKeybindEntry, emptyText, Vector2{(float)emptyX, (float)emptyY}, 20, 1, enabled ? GRAY : DARKGRAY);
    }
}

/**
 * @brief Gambar grid slot manual dan autosave
 * @param mousePosition Posisi mouse untuk efek hover
 *
 * Layout: 3+2 untuk manual, 3+2 untuk autosave.
 * Manual: slot 0-4, Autosave: slot 5-9.
 */
void SaveLoadScreen::DrawSlotGrid(Vector2 mousePosition)
{
    DrawTextEx(fontLoadingTitle, "MANUAL SAVE", Vector2{(float)(startX + 10), (float)(startY + 50)}, 22, 1, WHITE);

    int manualRow1Y = startY + 75;
    int rowWidth3 = 3 * SLOT_WIDTH + 2 * SLOT_GAP;
    int row1X = startX + (width - rowWidth3) / 2;

    for (int i = 0; i < 3; i++) {
        int slotX = row1X + i * (SLOT_WIDTH + SLOT_GAP);
        bool enabled = !(m_mode == SaveLoadMode::LOAD_MODE && !slotOccupied[i]);
        DrawSlotBox(i, slotX, manualRow1Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int manualRow2Y = manualRow1Y + SLOT_HEIGHT + SLOT_GAP;

    for (int i = 3; i < 6; i++) {
        int slotX = row1X + (i - 3) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled = !(m_mode == SaveLoadMode::LOAD_MODE && !slotOccupied[i]);
        DrawSlotBox(i, slotX, manualRow2Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int autoLabelY = manualRow2Y + SLOT_HEIGHT + 15;
    DrawTextEx(fontLoadingTitle, "AUTO SAVE", Vector2{(float)(startX + 10), (float)autoLabelY}, 22, 1, WHITE);

    int autoRow1Y = autoLabelY + 25;

    for (int i = 6; i < 9; i++) {
        int slotX = row1X + (i - 6) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled = !(m_mode == SaveLoadMode::SAVE_MODE); // Autosave disabled in save mode
        DrawSlotBox(i, slotX, autoRow1Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int autoRow2Y = autoRow1Y + SLOT_HEIGHT + SLOT_GAP;

    for (int i = 9; i < 12; i++) {
        int slotX = row1X + (i - 9) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled = !(m_mode == SaveLoadMode::SAVE_MODE); // Autosave disabled in save mode
        DrawSlotBox(i, slotX, autoRow2Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }
}

/**
 * @brief Muat metadata semua slot dari disk
 *
 * Untuk setiap slot N (0-9), periksa saves/slot_N/manual/manual.json.
 * Jika ada, baca mapDisplayName dan timestamp.
 * Jika tidak, tandai sebagai kosong.
 */
void SaveLoadScreen::RefreshSlotMetadata()
{
    for (int i = 0; i < MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT; i++)
    {
        std::string path = "saves/slot_" + std::to_string(i) + "/manual/manual.json";

        if (std::filesystem::exists(path))
        {
            slotOccupied[i] = true;
            try {
                std::ifstream file(path);
                nlohmann::json root;
                file >> root;

                slotMapName[i] = root.value("mapDisplayName", "Unknown");

                if (root.contains("timestamp")) {
                    slotTimestamp[i] = root["timestamp"].get<std::string>();
                } else {
                    auto ftime = std::filesystem::last_write_time(path);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
                    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

                    char timeStr[64];
                    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", std::localtime(&cftime));
                    slotTimestamp[i] = timeStr;
                }

                file.close();
            } catch (...) {
                slotMapName[i] = "Error";
                slotTimestamp[i] = "";
            }
        }
        else
        {
            slotOccupied[i] = false;
            slotMapName[i] = "";
            slotTimestamp[i] = "";
        }
    }
}
