/**
 * @file saveLoadScreen.cpp
 * @brief Menu Simpan/Muat Game Module
 *
 * Implementasi kelas SaveLoadScreen untuk menangani
 * UI menu simpan dan muat game.
 */

#include "ui/saveLoadScreen.h"
#include "core/game_state_saver.h"
#include "core/savemanager.h"
#include "core/seedmanager.h"
#include "map/worldgenio.h"
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
    : active(false), texturesLoaded(false), returnScreen(PLAY), width(0), height(0), startX(0), startY(0), bgTexture({0}), saveTitleTex({0}), loadTitleTex({0}), deleteTitleTex({0}), emptySlotTex({0}), savedSlotTex({0}), // basch-3: inisialisasi texture title & slot
      slotOccupied{}, slotMapName{}, slotTimestamp{}, m_mode(SaveLoadMode::SAVE_MODE), m_previousMode(SaveLoadMode::SAVE_MODE),       m_overwritePopup("Overwrite existing save?", "Overwrite", "Cancel", 0.7f), m_loadPopup("Load this save?", "Load", "Cancel", 0.7f), m_deletePopup("Delete this save?", "Delete", "Cancel", 0.7f), m_corruptionPopup("Save file corrupt!", "OK", 0.7f), m_showOverwritePopup(false), m_showLoadPopup(false), m_showDeletePopup(false), m_showCorruptionPopup(false), m_selectedSlot(-1)
{
}

/**
 * @brief Destructor
 *
 * Membersihkan resource texture background jika sudah dimuat.
 * @remarks basch-3: UnloadTextures() menangani semua texture termasuk title & slot
 */
SaveLoadScreen::~SaveLoadScreen()
{
    UnloadTextures();
}

/*==============================================================================
 * Public Methods
 *==============================================================================*/

/**
 * @brief Menampilkan layar save/load
 *
 * Mengaktifkan flag active dan menghitung ulang dimensi UI.
 * Texture background akan dimuat saat Show() pertama kali dipanggil.
 * @remarks basch-3: Memuat saveloadBG, title (save/load/delete), slot box, dan popup bg
 */
void SaveLoadScreen::Show()
{
    active = true;
    if (!texturesLoaded)
    {
        LoadTextures(); // basch-3: gantikan placeholder kosong
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
void SaveLoadScreen::Update(GameState *state, Vector2 mousePosition, bool mouseClicked)
{
    if (!active)
    {
        return;
    }

    // Handle overwrite popup
    if (m_showOverwritePopup)
    {
        m_overwritePopup.Update(mousePosition, mouseClicked);
        if (m_overwritePopup.IsConfirmClicked())
        {
            m_showOverwritePopup = false;
            SetActiveSlot(m_selectedSlot);
            // Save new-format snapshot
            {
                GameSnapshot snap = SaveManager::CaptureSnapshot();
                TraceLog(LOG_INFO, "MANUAL SAVE: slot=%d enemies=%zu items=%zu dead=%zu bomb=%zu crate=%zu playerPos=(%.0f,%.0f) mapPath='%s'",
                         m_selectedSlot, snap.enemies.size(), snap.items.size(), snap.deadEntities.size(),
                         snap.bombConsumed.size(), snap.crateConsumed.size(),
                         snap.playerPosition.x, snap.playerPosition.y, snap.mapPath.c_str());
                SaveManager::SaveManual(snap, m_selectedSlot);
            }
            active = false;
            state->currentScreen = returnScreen;
        }
        else if (!m_overwritePopup.IsActive())
        {
            m_showOverwritePopup = false; // Cancelled
        }
        return;
    }

    // Handle load popup
    if (m_showLoadPopup)
    {
        m_loadPopup.Update(mousePosition, mouseClicked);
        if (m_loadPopup.IsConfirmClicked())
        {
            m_showLoadPopup = false;
            SetActiveSlot(m_selectedSlot);

            // Cek validitas primary format (SaveManager snapshot)
            bool saveValid = false;
            std::string newPath = SaveManager::GetManualPath(m_selectedSlot);
            if (SaveManager::HasSnapshot(newPath))
            {
                GameSnapshot testSnap = SaveManager::ReadSnapshot(newPath);
                saveValid = (testSnap.version == GameSnapshot::SNAPSHOT_VERSION);
            }

            if (saveValid)
            {
                TraceLog(LOG_INFO, "LOAD: slot=%d snapshot valid", m_selectedSlot);
                active = false;
                state->currentScreen = LOADING;
            }
            else
            {
                TraceLog(LOG_WARNING, "LOAD: slot %d: snapshot corrupt atau tidak terbaca", m_selectedSlot);
                m_corruptionPopup.Show();
                m_showCorruptionPopup = true;
            }
        }
        else if (!m_loadPopup.IsActive())
        {
            m_showLoadPopup = false; // Cancelled
        }
        return;
    }

    // Handle corruption popup
    if (m_showCorruptionPopup)
    {
        m_corruptionPopup.Update(mousePosition, mouseClicked);
        if (m_corruptionPopup.IsConfirmClicked() || !m_corruptionPopup.IsActive())
        {
            m_showCorruptionPopup = false;
            m_selectedSlot = -1;
        }
        return;
    }

    // Handle delete popup
    if (m_showDeletePopup)
    {
        m_deletePopup.Update(mousePosition, mouseClicked);
        if (m_deletePopup.IsConfirmClicked())
        {
            m_showDeletePopup = false;
            SaveManager::DeleteSlot(m_selectedSlot);
            RefreshSlotMetadata();
            m_selectedSlot = -1;
        }
        else if (!m_deletePopup.IsActive())
        {
            m_showDeletePopup = false; // Cancelled
        }
        return;
    }

    // Handle slot clicks
    if (mouseClicked)
    {
        int clickedSlot = GetSlotAtPosition(mousePosition);
        if (clickedSlot >= 0)
        {
            if (m_mode == SaveLoadMode::SAVE_MODE)
            {
                // Autosave slots disabled in save mode
                if (clickedSlot >= MANUAL_SLOT_COUNT)
                {
                    return;
                }

                m_selectedSlot = clickedSlot;
                if (slotOccupied[clickedSlot])
                {
                    m_overwritePopup.Show();
                    m_showOverwritePopup = true;
                }
                else
                {
                    SetActiveSlot(clickedSlot);
                    // Save new-format snapshot
                    {
                        GameSnapshot snap = SaveManager::CaptureSnapshot();
                        SaveManager::SaveManual(snap, clickedSlot);
                    }
                    active = false;
                    state->currentScreen = returnScreen;
                }
            }
            else if (m_mode == SaveLoadMode::LOAD_MODE)
            {
                // Empty slots disabled in load mode
                if (!slotOccupied[clickedSlot])
                {
                    return;
                }

                m_selectedSlot = clickedSlot;
                m_loadPopup.Show();
                m_showLoadPopup = true;
            }
            else if (m_mode == SaveLoadMode::DELETE_MODE)
            {
                // Empty slots disabled in delete mode
                if (!slotOccupied[clickedSlot])
                {
                    return;
                }

                m_selectedSlot = clickedSlot;
                m_deletePopup.Show();
                m_showDeletePopup = true;
            }
            return; // Slot clicked, don't process backButton
        }
    }

    // DELETE button - switch to delete mode (abaikan jika sudah di DELETE_MODE)
    if (m_mode != SaveLoadMode::DELETE_MODE && deleteButton.isClicked(mousePosition, mouseClicked))
    {
        m_previousMode = m_mode;
        m_mode = SaveLoadMode::DELETE_MODE;
        return;
    }

    // Back button (one level up in DELETE_MODE, otherwise exit)
    if (backButton.isClicked(mousePosition, mouseClicked))
    {
        if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            m_mode = m_previousMode;
            return;
        }
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
    if (!active)
    {
        return;
    }

    DrawMenuBackground();

    if (bgTexture.id != 0)
    {
        DrawTexture(bgTexture, startX, startY, WHITE);
    }

    // basch-3: header diganti dgn texture title per mode (saveTitle / loadTitle / deleteTitle)
    Texture2D *headerTex = nullptr;
    if (m_mode == SaveLoadMode::SAVE_MODE)
    {
        headerTex = &saveTitleTex;
    }
    else if (m_mode == SaveLoadMode::LOAD_MODE)
    {
        headerTex = &loadTitleTex;
    }
    else if (m_mode == SaveLoadMode::DELETE_MODE)
    {
        headerTex = &deleteTitleTex;
    }

    if (headerTex != nullptr && headerTex->id != 0)
    {
        int texX = startX + (width - headerTex->width) / 2;
        int texY = startY + 80; // basch-3: offset 80
        DrawTexture(*headerTex, texX, texY, WHITE);
    }
    else
    {
        // Fallback: draw text header
        const char *headerText = "SAVE GAME";
        if (m_mode == SaveLoadMode::LOAD_MODE)
        {
            headerText = "LOAD GAME";
        }
        else if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            headerText = "DELETE SAVE";
        }
        int headerFontSize = 28;
        Vector2 headerTextSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), headerText, headerFontSize, 1);
        int headerX = startX + (int)(width - headerTextSize.x) / 2;
        DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), headerText, Vector2{(float)headerX, (float)(startY + 18)}, headerFontSize, 1, WHITE);
    }

    // Draw slot grid
    DrawSlotGrid(mousePosition);

    deleteButton.Draw(mousePosition);
    backButton.Draw(mousePosition);

    // Draw popups on top
    if (m_showOverwritePopup)
    {
        m_overwritePopup.Draw(mousePosition);
    }
    if (m_showLoadPopup)
    {
        m_loadPopup.Draw(mousePosition);
    }
    if (m_showDeletePopup)
    {
        m_deletePopup.Draw(mousePosition);
    }
    if (m_showCorruptionPopup)
    {
        m_corruptionPopup.Draw(mousePosition);
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
 *
 * @remarks basch-3: Panel diperbesar ke 1077x654 (saveloadBG.png native),
 *          SLOT_WIDTH=269, SLOT_HEIGHT=82 (savedBox.png native)
 */
void SaveLoadScreen::CalculateDimensions()
{
    width = 1077; // basch-3: dari 850 → 1077 (saveloadBG.png)
    height = 654; // basch-3: dari 500 → 654 (saveloadBG.png)
    startX = (GameScreenWidth - width) / 2;
    startY = (GameScreenHeight - height) / 2;

    backgroundRect = {
        static_cast<float>(startX),
        static_cast<float>(startY),
        static_cast<float>(width),
        static_cast<float>(height)};

    // basch-3: buttonImage untuk BACK (saveloadBack.png) dan DELETE (saveloadDelete.png)
    backButton = buttonImage(
        "assets/textures/saveloadAsset/saveloadBack.png",
        Vector2{static_cast<float>(startX + width - 135),
                static_cast<float>(startY + height - 60)},
        1.0F, 0.7F);

    deleteButton = buttonImage(
        "assets/textures/saveloadAsset/saveloadDelete.png",
        Vector2{static_cast<float>(startX + 130),
                static_cast<float>(startY + height - 60)},
        1.0F, 0.7F);
}

/**
 * @brief Dapatkan index slot berdasarkan posisi klik
 * @param mousePosition Posisi mouse
 * @return Index slot (0-9) atau -1 jika tidak ada slot di posisi tersebut
 */
int SaveLoadScreen::GetSlotAtPosition(Vector2 mousePosition)
{
    int manualRow1Y = startY + 160; // basch-3: offset 160
    int rowWidth3 = 3 * SLOT_WIDTH + 2 * SLOT_GAP;
    int row1X = startX + (width - rowWidth3) / 2;

    // Manual row 1 (slots 0, 1, 2)
    for (int i = 0; i < 3; i++)
    {
        int slotX = row1X + i * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(manualRow1Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect))
            return i;
    }

    int manualRow2Y = manualRow1Y + SLOT_HEIGHT + SLOT_GAP;

    // Manual row 2 (slots 3, 4, 5)
    for (int i = 3; i < 6; i++)
    {
        int slotX = row1X + (i - 3) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(manualRow2Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect))
            return i;
    }

    int autoLabelY = manualRow2Y + SLOT_HEIGHT + 15;
    int autoRow1Y = autoLabelY + 25;

    // Auto row 1 (slots 6, 7, 8)
    for (int i = 6; i < 9; i++)
    {
        int slotX = row1X + (i - 6) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(autoRow1Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect))
            return i;
    }

    int autoRow2Y = autoRow1Y + SLOT_HEIGHT + SLOT_GAP;

    // Auto row 2 (slots 9, 10, 11)
    for (int i = 9; i < 12; i++)
    {
        int slotX = row1X + (i - 9) * (SLOT_WIDTH + SLOT_GAP);
        Rectangle rect = {static_cast<float>(slotX), static_cast<float>(autoRow2Y), static_cast<float>(SLOT_WIDTH), static_cast<float>(SLOT_HEIGHT)};
        if (CheckCollisionPointRec(mousePosition, rect))
            return i;
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
void SaveLoadScreen::DrawSlotBox(int slotIndex, int posX, int posY, bool occupied, const std::string &mapName, const std::string &timestamp, Vector2 mousePosition, bool enabled)
{
    Rectangle slotRect = {
        static_cast<float>(posX),
        static_cast<float>(posY),
        static_cast<float>(SLOT_WIDTH),
        static_cast<float>(SLOT_HEIGHT)};

    bool hovered = enabled && CheckCollisionPointRec(mousePosition, slotRect);

    // basch-3: slot box texture — emptyBox (264x74) pas di slot, savedBox (269x82)
    //          dekorasi 8px atas & 6px kanan di-overflow di luar slot area
    bool useTexture = false;
    if (occupied && savedSlotTex.id != 0)
    {
        // SavedBox: offset Y -8 agar konten sejajar dgn slot, dekorasi overflow ke atas
        DrawTexture(savedSlotTex, posX, posY - 8, WHITE);
        useTexture = true;
    }
    else if (!occupied && emptySlotTex.id != 0)
    {
        // EmptyBox: ukuran sama persis dgn slot (264x74)
        DrawTexture(emptySlotTex, posX, posY, WHITE);
        useTexture = true;
    }

    if (useTexture)
    {
        // Overlay for hover / disabled state
        if (!enabled)
        {
            DrawRectangleRec(slotRect, ColorAlpha(BLACK, 0.4f));
        }
        else if (hovered)
        {
            DrawRectangleRec(slotRect, ColorAlpha(WHITE, 0.15f));
        }
    }
    else
    {
        // Fallback: colored rectangle + border
        Color bgColor;
        if (!enabled)
        {
            bgColor = {20, 20, 30, 140};
        }
        else if (hovered)
        {
            bgColor = {70, 70, 100, 220};
        }
        else if (occupied)
        {
            bgColor = {50, 50, 70, 220};
        }
        else
        {
            bgColor = {30, 30, 40, 180};
        }
        DrawRectangleRec(slotRect, bgColor);

        Color borderColor;
        if (!enabled)
        {
            borderColor = {60, 60, 70, 100};
        }
        else
        {
            borderColor = occupied ? (hovered ? WHITE : (Color){180, 180, 200, 255}) : GRAY;
        }
        DrawRectangleLinesEx(slotRect, 1, borderColor);
    }

    if (!enabled && slotIndex >= MANUAL_SLOT_COUNT && m_mode == SaveLoadMode::SAVE_MODE)
    {
        DrawTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), "Auto Save", Vector2{(float)(posX + 5), (float)(posY + 5)}, 16, 1, BLACK);
    }
    else
    {
        DrawTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), TextFormat("Slot %d", slotIndex), Vector2{(float)(posX + 5), (float)(posY + 5)}, 16, 1, BLACK);
    }

    if (occupied)
    {
        DrawTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), mapName.c_str(), Vector2{(float)(posX + 5), (float)(posY + 24)}, 18, 1, BLACK);
        DrawTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), timestamp.c_str(), Vector2{(float)(posX + 5), (float)(posY + 48)}, 14, 1, BLACK);
    }
    else
    {
        const char *emptyText = "Empty";
        Vector2 emptyTextSize = MeasureTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), emptyText, 20, 1);
        int emptyX = posX + (SLOT_WIDTH - (int)emptyTextSize.x) / 2;
        int emptyY = posY + (SLOT_HEIGHT - 20) / 2;
        DrawTextEx(GetOrLoad(FontId::SAVESLOT_TEXT), emptyText, Vector2{(float)emptyX, (float)emptyY}, 20, 1, BLACK);
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
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), "MANUAL SAVE", Vector2{(float)(startX + 70), (float)(startY + 135)}, 22, 1, BLACK); // basch-3: offset 135

    int manualRow1Y = startY + 160; // basch-3: offset 160
    int rowWidth3 = 3 * SLOT_WIDTH + 2 * SLOT_GAP;
    int row1X = startX + (width - rowWidth3) / 2;

    for (int i = 0; i < 3; i++)
    {
        int slotX = row1X + i * (SLOT_WIDTH + SLOT_GAP);
        bool enabled;
        if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            enabled = slotOccupied[i];
        }
        else
        {
            enabled = !(m_mode == SaveLoadMode::LOAD_MODE && !slotOccupied[i]);
        }
        DrawSlotBox(i, slotX, manualRow1Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int manualRow2Y = manualRow1Y + SLOT_HEIGHT + SLOT_GAP;

    for (int i = 3; i < 6; i++)
    {
        int slotX = row1X + (i - 3) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled;
        if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            enabled = slotOccupied[i];
        }
        else
        {
            enabled = !(m_mode == SaveLoadMode::LOAD_MODE && !slotOccupied[i]);
        }
        DrawSlotBox(i, slotX, manualRow2Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int autoLabelY = manualRow2Y + SLOT_HEIGHT + 15;
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), "AUTO SAVE", Vector2{(float)(startX + 70), (float)autoLabelY}, 22, 1, BLACK); // basch-3: X +60px total

    int autoRow1Y = autoLabelY + 25;

    for (int i = 6; i < 9; i++)
    {
        int slotX = row1X + (i - 6) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled;
        if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            enabled = slotOccupied[i];
        }
        else if (m_mode == SaveLoadMode::LOAD_MODE)
        {
            enabled = slotOccupied[i]; // Only occupied autosaves enabled in load mode
        }
        else
        {
            enabled = !(m_mode == SaveLoadMode::SAVE_MODE); // Autosave disabled in save mode
        }
        DrawSlotBox(i, slotX, autoRow1Y, slotOccupied[i], slotMapName[i], slotTimestamp[i], mousePosition, enabled);
    }

    int autoRow2Y = autoRow1Y + SLOT_HEIGHT + SLOT_GAP;

    for (int i = 9; i < 12; i++)
    {
        int slotX = row1X + (i - 9) * (SLOT_WIDTH + SLOT_GAP);
        bool enabled;
        if (m_mode == SaveLoadMode::DELETE_MODE)
        {
            enabled = slotOccupied[i];
        }
        else if (m_mode == SaveLoadMode::LOAD_MODE)
        {
            enabled = slotOccupied[i]; // Only occupied autosaves enabled in load mode
        }
        else
        {
            enabled = !(m_mode == SaveLoadMode::SAVE_MODE); // Autosave disabled in save mode
        }
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
        bool hasNewFormat = SaveManager::HasManual(i);

        if (hasNewFormat)
        {
            slotOccupied[i] = true;
            std::string metaPath = SaveManager::GetManualPath(i);
            try
            {
                std::ifstream file(metaPath);
                nlohmann::json root;
                file >> root;

                slotMapName[i] = root.value("mapDisplayName", "Unknown");

                if (root.contains("timestamp"))
                {
                    slotTimestamp[i] = root["timestamp"].get<std::string>();
                }
                else
                {
                    auto ftime = std::filesystem::last_write_time(metaPath);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
                    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

                    char timeStr[64];
                    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", std::localtime(&cftime));
                    slotTimestamp[i] = timeStr;
                }

                file.close();
            }
            catch (...)
            {
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

/*==============================================================================
 * Texture Management (basch-3: semua texture saveloadAsset dimuat di sini)
 *==============================================================================*/

void SaveLoadScreen::LoadTextures()
{
    // Unload existing textures first (safe if id==0)
    UnloadTextures();

    //-- Panel background
    Image imgBg = LoadImage("assets/textures/saveloadAsset/saveloadBG.png");
    if (imgBg.data != nullptr)
    {
        bgTexture = LoadTextureFromImage(imgBg);
        UnloadImage(imgBg);
    }

    //-- Title textures
    Image imgSave = LoadImage("assets/textures/saveloadAsset/saveTitle.png");
    if (imgSave.data != nullptr)
    {
        saveTitleTex = LoadTextureFromImage(imgSave);
        UnloadImage(imgSave);
    }

    Image imgLoad = LoadImage("assets/textures/saveloadAsset/loadTitle.png");
    if (imgLoad.data != nullptr)
    {
        loadTitleTex = LoadTextureFromImage(imgLoad);
        UnloadImage(imgLoad);
    }

    Image imgDelete = LoadImage("assets/textures/saveloadAsset/deleteTitle.png");
    if (imgDelete.data != nullptr)
    {
        deleteTitleTex = LoadTextureFromImage(imgDelete);
        UnloadImage(imgDelete);
    }

    //-- Slot textures
    Image imgEmpty = LoadImage("assets/textures/saveloadAsset/emptyBox.png");
    if (imgEmpty.data != nullptr)
    {
        emptySlotTex = LoadTextureFromImage(imgEmpty);
        UnloadImage(imgEmpty);
    }

    Image imgSaved = LoadImage("assets/textures/saveloadAsset/savedBox.png");
    if (imgSaved.data != nullptr)
    {
        savedSlotTex = LoadTextureFromImage(imgSaved);
        UnloadImage(imgSaved);
    }

    //-- Popup backgrounds & text color
    m_overwritePopup.SetBackgroundTexture("assets/textures/saveloadAsset/overLoadNotifBG.png");
    m_overwritePopup.SetTextColor(BLACK);
    m_loadPopup.SetBackgroundTexture("assets/textures/saveloadAsset/overLoadNotifBG.png");
    m_loadPopup.SetTextColor(BLACK);
    m_deletePopup.SetBackgroundTexture("assets/textures/saveloadAsset/overLoadNotifBG.png");
    m_deletePopup.SetTextColor(BLACK);
    m_corruptionPopup.SetBackgroundTexture("assets/textures/saveloadAsset/overLoadNotifBG.png");
    m_corruptionPopup.SetTextColor(BLACK);
}

// basch-3: bongkar semua texture (bg, title, slot)
void SaveLoadScreen::UnloadTextures()
{
    if (bgTexture.id != 0)
    {
        UnloadTexture(bgTexture);
        bgTexture = {0};
    }
    if (saveTitleTex.id != 0)
    {
        UnloadTexture(saveTitleTex);
        saveTitleTex = {0};
    }
    if (loadTitleTex.id != 0)
    {
        UnloadTexture(loadTitleTex);
        loadTitleTex = {0};
    }
    if (deleteTitleTex.id != 0)
    {
        UnloadTexture(deleteTitleTex);
        deleteTitleTex = {0};
    }
    if (emptySlotTex.id != 0)
    {
        UnloadTexture(emptySlotTex);
        emptySlotTex = {0};
    }
    if (savedSlotTex.id != 0)
    {
        UnloadTexture(savedSlotTex);
        savedSlotTex = {0};
    }
}
