/**
 * @file audioTab.cpp
 * @brief Implementasi Audio Settings Tab
 *
 * Handle rendering dan input untuk 3 slider volume (Master, Music, SFX)
 * dengan persistence ke JSON dan real-time audio control via AudioManager.
 */

#include "audioTab.h"
#include "fonts.h"
#include "../../include/systems/audioManager.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/raylib/include/raymath.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

/*==============================================================================
 * Static State
 *==============================================================================*/

/** @brief Global slider state untuk tab Audio */
SliderState g_sliders = {100, 80, 100, false, -1};

/*==============================================================================
 * Constants
 *==============================================================================*/

/** @brief Lebar slider bar dalam pixel */
static const int SLIDER_WIDTH = 250;

/** @brief Tinggi slider bar dalam pixel */
static const int SLIDER_HEIGHT = 20;

/** @brief Warna background slider */
static const Color SLIDER_BG    = {80, 80, 80, 255};

/** @brief Warna fill slider (hijau) */
static const Color SLIDER_FILL  = {50, 200, 50, 255};

/** @brief Warna border saat hover */
static const Color SLIDER_HOVER = {255, 255, 255, 120};

/** @brief Posisi X label */
static const int LABEL_X = 180;

/** @brief Posisi X slider bar */
static const int SLIDER_BAR_X = 380;

/** @brief Posisi X value text */
static const int VALUE_X = 660;

/** @brief Font size untuk label dan value */
static const int FONT_SIZE = 30;

/** @brief Row offset per slider */
static const int ROW_OFFSETS[3] = {15, 75, 135};

/** @brief Label teks untuk tiap slider */
static const char* SLIDER_LABELS[3] = {
    "Master Volume",
    "Music Volume",
    "SFX Volume"
};

/*==============================================================================
 * Draw Functions
 *==============================================================================*/

/**
 * @brief Me-render satu slider bar
 * @param label Teks label slider
 * @param valuePct Nilai persen 0-100
 * @param barX Posisi X slider bar
 * @param barY Posisi Y slider bar
 * @param mousePosition Posisi mouse untuk hover
 */
static void DrawSliderBar(
    const char* label,
    int valuePct,
    int barX,
    int barY,
    Vector2 mousePosition)
{
    // Label
    DrawTextEx(fontLoadingTitle, label,
        Vector2{static_cast<float>(LABEL_X), static_cast<float>(barY - 5)},
        FONT_SIZE, 0, WHITE);

    // Background bar
    DrawRectangle(barX, barY, SLIDER_WIDTH, SLIDER_HEIGHT, SLIDER_BG);

    // Fill bar (proporsional)
    int fillWidth = (valuePct * SLIDER_WIDTH) / 100;
    if (fillWidth > 0)
    {
        DrawRectangle(barX, barY, fillWidth, SLIDER_HEIGHT, SLIDER_FILL);
    }

    // value text pakai fontLoadingTitle (bold) menggantikan fontKeybindEntry
    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%d%%", valuePct);
    Vector2 textSize = MeasureTextEx(fontLoadingTitle, valueStr, FONT_SIZE, 0);
    float valX = barX + (SLIDER_WIDTH - textSize.x) * 0.5f;
    float valY = barY + (SLIDER_HEIGHT - textSize.y) * 0.5f;
    DrawTextEx(fontLoadingTitle, valueStr,
        Vector2{valX, valY}, FONT_SIZE, 0, BLACK);

    // Hover effect
    Rectangle sliderRect = {
        static_cast<float>(barX),
        static_cast<float>(barY),
        static_cast<float>(SLIDER_WIDTH),
        static_cast<float>(SLIDER_HEIGHT)
    };
    if (CheckCollisionPointRec(mousePosition, sliderRect))
    {
        DrawRectangleLines(barX, barY, SLIDER_WIDTH, SLIDER_HEIGHT, SLIDER_HOVER);
    }
}

/*==============================================================================
 * Public Draw
 *==============================================================================*/

void DrawAudioTab(
    Vector2 mousePosition,
    int startX,
    int startY)
{
    int contentStartY = startY + 100;
    int barX = startX + SLIDER_BAR_X;

    for (int i = 0; i < 3; i++)
    {
        int barY = contentStartY + ROW_OFFSETS[i];
        int value = (i == 0) ? g_sliders.masterVolume :
                    (i == 1) ? g_sliders.musicVolume :
                               g_sliders.sfxVolume;

        DrawSliderBar(SLIDER_LABELS[i], value, barX, barY, mousePosition);
    }
}

/*==============================================================================
 * Update
 *==============================================================================*/

bool UpdateAudioTab(
    SliderState& sliders,
    Vector2 mousePosition,
    bool mouseClicked,
    int startX,
    int startY)
{
    int contentStartY = startY + 100;
    int barX = startX + SLIDER_BAR_X;

    auto calcVolumeFromMouse = [&](int index) -> int
    {
        int barY = contentStartY + ROW_OFFSETS[index];
        float relX = mousePosition.x - static_cast<float>(barX);
        float pct = (relX / SLIDER_WIDTH) * 100.0f;
        int vol = static_cast<int>(pct + 0.5f);
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        return vol;
    };

    auto getSliderUnderMouse = [&]() -> int
    {
        for (int i = 0; i < 3; i++)
        {
            int barY = contentStartY + ROW_OFFSETS[i];
            Rectangle rect = {
                static_cast<float>(barX),
                static_cast<float>(barY),
                static_cast<float>(SLIDER_WIDTH),
                static_cast<float>(SLIDER_HEIGHT)
            };
            if (CheckCollisionPointRec(mousePosition, rect))
                return i;
        }
        return -1;
    };

    // Initiate drag on click
    if (mouseClicked && !sliders.isDragging)
    {
        int idx = getSliderUnderMouse();
        if (idx >= 0)
        {
            sliders.isDragging = true;
            sliders.dragIndex = idx;

            int newVol = calcVolumeFromMouse(idx);
            switch (idx)
            {
                case 0: sliders.masterVolume = newVol; break;
                case 1: sliders.musicVolume = newVol; break;
                case 2: sliders.sfxVolume = newVol; break;
            }

            AudioManager::SetVolumesFromPct(
                sliders.masterVolume,
                sliders.musicVolume,
                sliders.sfxVolume
            );

            TraceLog(LOG_INFO, "AUDIO: Slider %d digeser ke %d%%", idx, newVol);
        }
    }

    // Drag update — real-time volume change
    if (sliders.isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        int idx = sliders.dragIndex;
        if (idx >= 0)
        {
            int newVol = calcVolumeFromMouse(idx);
            switch (idx)
            {
                case 0: sliders.masterVolume = newVol; break;
                case 1: sliders.musicVolume = newVol; break;
                case 2: sliders.sfxVolume = newVol; break;
            }

            AudioManager::SetVolumesFromPct(
                sliders.masterVolume,
                sliders.musicVolume,
                sliders.sfxVolume
            );
        }
    }

    // Mouse release — simpan settings
    if (sliders.isDragging && IsMouseButtonUp(MOUSE_BUTTON_LEFT))
    {
        sliders.isDragging = false;
        sliders.dragIndex = -1;

        SaveAudioSettings(
            sliders.masterVolume,
            sliders.musicVolume,
            sliders.sfxVolume
        );

        TraceLog(LOG_INFO, "AUDIO: Settings disimpan (M=%d, Mu=%d, S=%d)",
            sliders.masterVolume,
            sliders.musicVolume,
            sliders.sfxVolume
        );

        return true;
    }

    return false;
}

/*==============================================================================
 * Load / Save
 *==============================================================================*/

bool LoadAudioSettings()
{
    if (!std::filesystem::exists(AUDIO_SETTINGS_PATH))
    {
        TraceLog(LOG_INFO, "AUDIO: Tidak ada file settings di %s, pakai default", AUDIO_SETTINGS_PATH);
        return false;
    }

    try
    {
        std::ifstream file(AUDIO_SETTINGS_PATH);
        json root = json::parse(file);

        if (!root.contains("version") || root.at("version").get<int>() != 1)
        {
            TraceLog(LOG_WARNING, "AUDIO: Version mismatch, pakai default");
            return false;
        }

        int master = root.value("masterVolume", 100);
        int music  = root.value("musicVolume", 80);
        int sfx    = root.value("sfxVolume", 100);

        // Clamp ke 0-100
        if (master < 0) master = 0;
        if (master > 100) master = 100;
        if (music < 0) music = 0;
        if (music > 100) music = 100;
        if (sfx < 0) sfx = 0;
        if (sfx > 100) sfx = 100;

        // Apply ke AudioManager
        AudioManager::SetVolumesFromPct(master, music, sfx);

        // Sync slider state
        g_sliders.masterVolume = master;
        g_sliders.musicVolume = music;
        g_sliders.sfxVolume = sfx;

        TraceLog(LOG_INFO, "AUDIO: Settings dimuat dari %s (M=%d, Mu=%d, S=%d)",
            AUDIO_SETTINGS_PATH, master, music, sfx);

        return true;
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_WARNING, "AUDIO: Gagal muat %s: %s", AUDIO_SETTINGS_PATH, e.what());
        return false;
    }
}

bool SaveAudioSettings(int masterVolume, int musicVolume, int sfxVolume)
{
    try
    {
        json root;
        root["version"]      = 1;
        root["masterVolume"] = masterVolume;
        root["musicVolume"]  = musicVolume;
        root["sfxVolume"]    = sfxVolume;

        std::filesystem::path fsPath(AUDIO_SETTINGS_PATH);
        std::filesystem::create_directories(fsPath.parent_path());

        std::string tmpPath = std::string(AUDIO_SETTINGS_PATH) + ".tmp";
        if (std::filesystem::exists(tmpPath))
            std::filesystem::remove(tmpPath);

        {
            std::ofstream file(tmpPath);
            file << root.dump(2);
        }

        std::filesystem::rename(tmpPath, AUDIO_SETTINGS_PATH);
        TraceLog(LOG_INFO, "AUDIO: Settings disimpan ke %s", AUDIO_SETTINGS_PATH);
        return true;
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_WARNING, "AUDIO: Gagal simpan: %s", e.what());
        return false;
    }
}
