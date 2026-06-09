#pragma once

#include "raylib.h"
#include "button.h"
#include "screen.h"
#include "popup.h"
#include <string>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

enum class SaveLoadMode {
    SAVE_MODE,
    LOAD_MODE
};

class SaveLoadScreen {
public:
    SaveLoadScreen();
    ~SaveLoadScreen();

    void Show();
    void Hide();
    bool IsActive() const;

    void Update(GameState* state, Vector2 mousePosition, bool mouseClicked);
    void Draw(Vector2 mousePosition);

    void SetReturnScreen(ScreenState screen);
    void SetMode(SaveLoadMode mode);

private:
    void CalculateDimensions();
    int GetSlotAtPosition(Vector2 mousePosition);
    void DrawSlotBox(int slotIndex, int posX, int posY, bool occupied, const std::string& mapName, const std::string& timestamp, Vector2 mousePosition, bool enabled);
    void DrawSlotGrid(Vector2 mousePosition);
    void RefreshSlotMetadata(void);

    static constexpr int MANUAL_SLOT_COUNT = 6;
    static constexpr int AUTOSAVE_SLOT_COUNT = 6;
    static constexpr int SLOT_WIDTH = 250;
    static constexpr int SLOT_HEIGHT = 70;
    static constexpr int SLOT_GAP = 10;

    bool active;
    bool texturesLoaded;
    ScreenState returnScreen;
    buttonTxt backButton;
    int width;
    int height;
    int startX;
    int startY;
    Rectangle backgroundRect;
    Texture2D bgTexture;
    bool slotOccupied[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];
    std::string slotMapName[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];
    std::string slotTimestamp[MANUAL_SLOT_COUNT + AUTOSAVE_SLOT_COUNT];
    SaveLoadMode m_mode;
    Popup m_overwritePopup;
    Popup m_loadPopup;
    bool m_showOverwritePopup;
    bool m_showLoadPopup;
    int m_selectedSlot;
};
