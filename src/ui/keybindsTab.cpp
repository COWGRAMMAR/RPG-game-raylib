#include "keybindsTab.h"
#include "fonts.h"
#include "keybindManager.h"
#include "../lib/raylib/include/raylib.h"
#include <algorithm>

static const char* SAVE_PATH = "saves/settings/keybindsTab.json";

const char* GetKeybindsSettingsPath()
{
    return SAVE_PATH;
}

struct SectionInfo {
    const char* title;
    Color color;
    int startAction;
    int actionCount;
};

static const SectionInfo sections[] = {
    {"MOVEMENT",   YELLOW,  0, 4},
    {"COMBAT",     YELLOW,  4, 3},
    {"INVENTORY",  YELLOW,  7, 4},
    {"HOTBAR",     YELLOW, 11, 4},
};

static const int SECTION_COUNT = sizeof(sections) / sizeof(sections[0]);

static const int HEADER_HEIGHT  = 50;
static const int ROW_HEIGHT     = 36;
static const int COL_X          = 40;
static const int KEY_COL_W      = 180;
static const int SEP_W          = 20;
static const int HITBOX_PAD     = 2;

/// Visible content area (inside the dark overlay)
static const int CONTENT_TOP    = 90;    // from startY (contentStartY = passed startY + 90)
static const int CONTENT_H      = 292;   // visible height (screen Y 547 - screen Y 255)

static bool IsInside(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void DrawKeybindsTab(Vector2 mousePosition, int startX, int startY)
{
    int mx = static_cast<int>(mousePosition.x);
    int my = static_cast<int>(mousePosition.y);

    static int scrollY = 0;
    static int listeningAction = -1;
    static bool enteredThisFrame = false;

    // Calculate total content height
    int totalH = 0;
    for (int si = 0; si < SECTION_COUNT; si++)
    {
        totalH += HEADER_HEIGHT;
        totalH += sections[si].actionCount * ROW_HEIGHT;
        totalH += ROW_HEIGHT;
    }

    int contentStartY = startY + CONTENT_TOP;
    int maxScroll = std::max(0, totalH - CONTENT_H);

    scrollY -= static_cast<int>(GetMouseWheelMove()) * ROW_HEIGHT;
    scrollY = std::clamp(scrollY, 0, maxScroll);

    auto screenY = [&](int localY) { return contentStartY + localY - scrollY; };
    auto isVisible = [&](int localY, int h) -> bool {
        int top = screenY(localY);
        int bottom = top + h;
        return bottom > contentStartY && top < contentStartY + CONTENT_H;
    };

    // ---- Clip to content area ----
    BeginScissorMode(startX, contentStartY, 600, CONTENT_H);

    // ---- Handle rebind input ----
    if (listeningAction >= 0)
    {
        if (enteredThisFrame)
        {
            while (GetKeyPressed() != 0) {}
            enteredThisFrame = false;
        }

        int key = GetKeyPressed();
        if (key != 0)
        {
            if (key == KEY_ESCAPE)
                listeningAction = -1;
            else
            {
                keybindManager.SetKeybind(static_cast<Action>(listeningAction), key, false);
                keybindManager.SaveToFile(SAVE_PATH);
                listeningAction = -1;
            }
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            keybindManager.SetKeybind(static_cast<Action>(listeningAction), MOUSE_BUTTON_LEFT, true);
            keybindManager.SaveToFile(SAVE_PATH);
            listeningAction = -1;
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            keybindManager.SetKeybind(static_cast<Action>(listeningAction), MOUSE_BUTTON_RIGHT, true);
            keybindManager.SaveToFile(SAVE_PATH);
            listeningAction = -1;
        }
    }

    // ---- Render ----
    int currentLocalY = 0;

    for (int si = 0; si < SECTION_COUNT; si++)
    {
        const SectionInfo& sec = sections[si];

        if (isVisible(currentLocalY, HEADER_HEIGHT))
        {
            DrawTextEx(fontKeybindHeader, sec.title,
                Vector2{(float)(startX + COL_X), (float)screenY(currentLocalY)},
                32, 0, sec.color);
        }
        currentLocalY += HEADER_HEIGHT;

        for (int ai = 0; ai < sec.actionCount; ai++)
        {
            if (!isVisible(currentLocalY, ROW_HEIGHT))
            {
                currentLocalY += ROW_HEIGHT;
                continue;
            }

            int y = screenY(currentLocalY);
            Action action = static_cast<Action>(sec.startAction + ai);

            int keyBoxX = startX + COL_X;
            int keyBoxY = y;
            int keyBoxW = KEY_COL_W;
            int keyBoxH = ROW_HEIGHT;

            bool hovered = IsInside(mx, my, keyBoxX - HITBOX_PAD, keyBoxY - HITBOX_PAD,
                                    keyBoxW + HITBOX_PAD * 2, keyBoxH + HITBOX_PAD * 2);

            bool isListening = (listeningAction == static_cast<int>(action));
            Color bgColor = isListening ? Color{40, 80, 40, 255}
                         : hovered ? Color{50, 50, 50, 255}
                         : BLANK;

            if (bgColor.a > 0)
                DrawRectangle(keyBoxX, keyBoxY, keyBoxW, keyBoxH, bgColor);

            const char* keyName = keybindManager.GetKeyDisplayName(action);
            Color keyColor = isListening ? GREEN : YELLOW;

            DrawTextEx(fontKeybindEntry, keyName,
                Vector2{(float)keyBoxX, (float)y},                 30, 0, keyColor);
            DrawTextEx(fontKeybindEntry, "=>",
                Vector2{(float)(keyBoxX + KEY_COL_W), (float)y}, 30, 0, GRAY);
            DrawTextEx(fontKeybindEntry, keybindManager.GetActionName(action),
                Vector2{(float)(keyBoxX + KEY_COL_W + SEP_W), (float)y}, 30, 0, WHITE);

            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && listeningAction != static_cast<int>(action))
            {
                listeningAction = static_cast<int>(action);
                enteredThisFrame = true;
            }

            currentLocalY += ROW_HEIGHT;
        }

        currentLocalY += ROW_HEIGHT;
    }

    EndScissorMode();

    // Scroll indicators
    if (maxScroll > 0)
    {
        int indX = startX + COL_X;
        if (scrollY > 0)
            DrawTextEx(fontKeybindEntry, "^^^",
                Vector2{(float)indX, (float)(contentStartY - 2)}, 26, 0, GRAY);
        if (scrollY < maxScroll)
            DrawTextEx(fontKeybindEntry, "vvv",
                Vector2{(float)indX, (float)(contentStartY + CONTENT_H - 26)}, 26, 0, GRAY);
    }

    // Listening popup
    if (listeningAction >= 0)
    {
        const int POPUP_W = 420;
        const int POPUP_H = 80;
        const int popupX = startX + (800 - POPUP_W) / 2;
        const int popupY = startY + (600 - POPUP_H) / 2 - 30;

        DrawRectangle(popupX, popupY, POPUP_W, POPUP_H, Color{20, 20, 30, 235});
        DrawRectangleLinesEx(Rectangle{(float)popupX, (float)popupY, (float)POPUP_W, (float)POPUP_H}, 2, GREEN);

        const char* line1 = "Press a key or click a mouse button.";
        const char* line2 = "ESC to cancel.";
        Vector2 sz1 = MeasureTextEx(fontKeybindEntry, line1, 30, 0);
        Vector2 sz2 = MeasureTextEx(fontKeybindEntry, line2, 30, 0);
        DrawTextEx(fontKeybindEntry, line1,
            Vector2{(float)(popupX + (POPUP_W - sz1.x) / 2), (float)(popupY + 8)},
            30, 0, WHITE);
        DrawTextEx(fontKeybindEntry, line2,
            Vector2{(float)(popupX + (POPUP_W - sz2.x) / 2), (float)(popupY + 44)},
            30, 0, GREEN);
    }
}
