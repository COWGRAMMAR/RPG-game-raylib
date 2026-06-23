/**
 * @file videoTab.cpp
 * @brief Implementasi Video Settings Tab
 *
 * Handle rendering dan input untuk pengaturan video.
 */

#include "videoTab.h"
#include "fonts.h"
#include "screen.h"
#include "raylib.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

/*==============================================================================
 * Constants
 *==============================================================================*/

/** @brief Warna teks label settings (dark brown, sama kayak audio/keybind tab) */
static const Color LABEL_COLOR = {31, 31, 28, 255};

/** @brief Posisi X label */
static const int LABEL_X_OFFSET = 40;

/** @brief Font size untuk label */
static const int FONT_SIZE = 34;

/** @brief Row Y offsets dari contentStartY */
static const int ROW_OFFSETS[2] = {12, 72};

/** @brief Label teks untuk tiap toggle */
static const char *TOGGLE_LABELS[2] = {
    "FULLSCREEN",
    "SHOW FPS"};

void DrawVideoTab(
    buttonTxt &fullscreenButton,
    buttonTxt &fpsButton,
    Vector2 mousePosition,
    int startX,
    int startY)
{
    int contentStartY = startY + 100;
    int labelX = startX + LABEL_X_OFFSET;
    FontHandle vfxFont = {FontId::VIDEOSETTS_LABEL, AtlasRes::RES_512};

    for (int i = 0; i < 2; i++)
    {
        DrawTextCached(
            vfxFont,
            TOGGLE_LABELS[i],
            Vector2{(float)labelX, (float)(contentStartY + ROW_OFFSETS[i])},
            FONT_SIZE, 0, LABEL_COLOR);
    }

    fullscreenButton.Draw(mousePosition);
    fpsButton.Draw(mousePosition);
}

bool UpdateVideoTab(
    buttonTxt &fullscreenButton,
    buttonTxt &fpsButton,
    void *stateVoid,
    Vector2 mousePosition,
    bool mouseClicked)
{
    auto *state = static_cast<GameState *>(stateVoid);

    if (fullscreenButton.isClicked(mousePosition, mouseClicked))
    {
        if (IsWindowFullscreen())
        {
            ToggleFullscreenMode();
        }
        else
        {
            Rectangle monitorRes = GetMonitorResolution();
            SetWindowSize(
                static_cast<int>(monitorRes.width),
                static_cast<int>(monitorRes.height));
            ToggleFullscreenMode();
        }
        SaveVideoSettings(IsWindowFullscreen(), state->showFPS);
        return true;
    }

    if (fpsButton.isClicked(mousePosition, mouseClicked))
    {
        state->showFPS = !state->showFPS;
        SaveVideoSettings(IsWindowFullscreen(), state->showFPS);
        return true;
    }

    return false;
}

bool LoadVideoSettings(void *stateVoid)
{
    auto *state = static_cast<GameState *>(stateVoid);

    if (!std::filesystem::exists(VIDEO_SETTINGS_PATH))
    {
        TraceLog(LOG_INFO, "VIDEO: Tidak ada file settings di %s, pakai default", VIDEO_SETTINGS_PATH);
        return false;
    }

    try
    {
        std::ifstream file(VIDEO_SETTINGS_PATH);
        json root = json::parse(file);

        if (!root.contains("version") || root.at("version").get<int>() != 1)
        {
            TraceLog(LOG_WARNING, "VIDEO: Version mismatch, pakai default");
            return false;
        }

        if (root.contains("fullscreen"))
        {
            bool fs = root["fullscreen"].get<bool>();
            if (fs && !IsWindowFullscreen())
            {
                Rectangle monitorRes = GetMonitorResolution();
                SetWindowSize(
                    static_cast<int>(monitorRes.width),
                    static_cast<int>(monitorRes.height));
                ToggleFullscreenMode();
            }
        }

        if (root.contains("showFPS"))
            state->showFPS = root["showFPS"].get<bool>();

        TraceLog(LOG_INFO, "VIDEO: Settings dimuat dari %s", VIDEO_SETTINGS_PATH);
        return true;
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "VIDEO: Gagal muat %s: %s", VIDEO_SETTINGS_PATH, e.what());
        return false;
    }
}

bool SaveVideoSettings(bool fullscreen, bool showFPS)
{
    try
    {
        json root;
        root["version"] = 1;
        root["fullscreen"] = fullscreen;
        root["showFPS"] = showFPS;

        std::filesystem::path fsPath(VIDEO_SETTINGS_PATH);
        std::filesystem::create_directories(fsPath.parent_path());

        std::string tmpPath = std::string(VIDEO_SETTINGS_PATH) + ".tmp";
        if (std::filesystem::exists(tmpPath))
            std::filesystem::remove(tmpPath);

        {
            std::ofstream file(tmpPath);
            file << root.dump(2);
        }

        std::filesystem::rename(tmpPath, VIDEO_SETTINGS_PATH);
        TraceLog(LOG_INFO, "VIDEO: Settings disimpan ke %s", VIDEO_SETTINGS_PATH);
        return true;
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_WARNING, "VIDEO: Gagal simpan: %s", e.what());
        return false;
    }
}
