#include "keybindsTab.h"
#include "fonts.h"
#include "keybindManager.h"
#include "screen.h"
#include "raylib.h"
#include <algorithm>

static const char *SAVE_PATH = "saves/settings/keybindsTab.json";

const char *GetKeybindsSettingsPath()
{
    return SAVE_PATH;
}

/**
 * @brief Informational entry (combo display, not rebindable)
 */
struct InfoEntry
{
    const char *actionName;
    const char *comboDisplay; ///< e.g. "Ctrl + Click"
};

// Combo-only entries di Inventory: Drag, Split, Merge (setelah HOTBAR_SLOT_4)
static const InfoEntry inventoryInfo[] = {
    {" Drag Item", "Left Click + Drag"},
    {" Split Item", "Ctrl + Click"},
    {" Merge Item", "Right Click + Drag"},
};

struct SectionInfo
{
    const char *title;
    Color color;
    const int *actionIndices;
    int actionCount;
    const InfoEntry *infoEntries;
    int infoCount;
};

// Action indices per section (non-contiguous di enum, jadi pake explicit array)
static const int movementIndices[] = {0, 1, 2, 3};         // MOVE_UP, DOWN, LEFT, RIGHT
static const int actionIndices[] = {9, 10, 4, 7, 8, 6};    // ATTACK_DRINK, DASH, INTERACT, DROP_ITEM, DROP_ALL, TOGGLE_MAP
static const int inventoryIndices[] = {5, 11, 12, 13, 14}; // TOGGLE_INVENTORY, HOTBAR_SLOT_1,2,3,4

static const SectionInfo sections[] = {
    {"MOVEMENT", WHITE, movementIndices, 4, nullptr, 0},
    {"ACTION", WHITE, actionIndices, 6, nullptr, 0},
    {"INVENTORY", WHITE, inventoryIndices, 5, inventoryInfo, 3},
};

static const int SECTION_COUNT = sizeof(sections) / sizeof(sections[0]);

/*=== Layout Constants ===*/
static constexpr int HEADER_HEIGHT = 50;      // tinggi header tiap section
static constexpr int ROW_HEIGHT = 36;         // tinggi tiap baris entry
static constexpr int COL_X = 40;              // padding kiri konten dari tepi panel
static constexpr int HITBOX_PAD = 2;          // padding ekstra hitbox hover
static constexpr int CONTENT_TOP = 90;        // posisi Y awal konten dari startY
static constexpr int CONTENT_H = 292;         // tinggi area konten terlihat (scissor)
static constexpr int KEYB_FONT_SZ = 28;       // font entry (Poppins-Regular)
static constexpr int KEYB_HEADER_SZ = 32;     // font header (NewDawn)
static constexpr int KEYB_TOAST_SZ = 30;      // font toast notifikasi
static constexpr int NAME_PAD = 10;           // padding action name -> separator
static constexpr int SEP_GAP = 40;            // jarak separator -> key name
static constexpr float TOAST_DURATION = 3.5f; // durasi toast (detik)
static constexpr int SCROLLBAR_W = 14;        // lebar scrollbar vertikal
static constexpr int POPUP_W = 420;           // lebar popup listening
static constexpr int POPUP_H = 80;            // tinggi popup listening

static bool IsInside(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

/*=== Helpers ===*/

/**
 * @brief Hitung lebar nama action terpanjang buat tab-stop alignment
 */
static float CalculateMaxNameWidth()
{
    float maxW = 0.0f;
    for (int si = 0; si < SECTION_COUNT; si++)
    {
        const SectionInfo &sec = sections[si];
        for (int ai = 0; ai < sec.actionCount; ai++)
        {
            Action a = static_cast<Action>(sec.actionIndices[ai]);
            Vector2 sz = MeasureTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), keybindManager.GetActionName(a), KEYB_FONT_SZ, 0);
            if (sz.x > maxW)
                maxW = sz.x;
        }
        for (int ii = 0; ii < sec.infoCount; ii++)
        {
            Vector2 sz = MeasureTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), sec.infoEntries[ii].actionName, KEYB_FONT_SZ, 0);
            if (sz.x > maxW)
                maxW = sz.x;
        }
    }
    return maxW;
}

/**
 * @brief Coba rebind action, handle conflict swap + toast. Returns true jika ada perubahan
 * @param listeningAction Nilai Action yang lagi di-rebind
 * @param keyCode Keycode atau MOUSE_BUTTON_LEFT/RIGHT
 * @param isMouse Kalo true berarti mouse button
 * @param inputName Nama display buat toast (e.g. "Q", "Mouse Left")
 */
static bool TryRebind(int listeningAction, int keyCode, bool isMouse,
                      const char *inputName,
                      char toastLine1[128], char toastLine2[128], float &toastTimer)
{
    Action curAction = static_cast<Action>(listeningAction);
    Action targetAction = (curAction == DROP_ALL) ? DROP_ITEM : curAction;
    Action conflictAction = keybindManager.FindActionByKeycode(keyCode, isMouse);

    // DROP_ALL combo: Left Ctrl sebagai modifier — skip silent
    if (curAction == DROP_ALL && conflictAction == DROP_ALL)
        return false;

    if (conflictAction != ACTION_COUNT && conflictAction != targetAction)
    {
        Keybind oldKey = keybindManager.GetKeybind(targetAction);
        keybindManager.SetKeybind(conflictAction, oldKey.keyCode, oldKey.isMouse);
        keybindManager.SetKeybind(targetAction, keyCode, isMouse);

        const char *cName = keybindManager.GetActionName(conflictAction);
        const char *lName = keybindManager.GetActionName(curAction);
        snprintf(toastLine1, 128, "%s: %s \xE2\x86\x92 %s", inputName, cName, lName);
        const char *oldKeyName = KeybindManager::GetInputDisplayName(oldKey.keyCode, oldKey.isMouse);
        snprintf(toastLine2, 128, "%s: \xE2\x86\x92 %s", oldKeyName, cName);
        toastTimer = TOAST_DURATION;
    }
    else
    {
        keybindManager.SetKeybind(targetAction, keyCode, isMouse);
    }
    keybindManager.SaveToFile(SAVE_PATH);
    return true;
}

/**
 * @brief Handle listening input: keyboard + mouse left + mouse right
 */
static void HandleRebindInput(int &listeningAction, bool &enteredThisFrame,
                              char toastLine1[128], char toastLine2[128], float &toastTimer)
{
    if (listeningAction < 0)
        return;

    // Frame pertama setelah masuk mode listen: flush keyboard buffer
    if (enteredThisFrame)
    {
        while (GetKeyPressed() != 0)
        {
        }
        enteredThisFrame = false;
    }

    int key = GetKeyPressed();
    if (key != 0)
    {
        if (key == KEY_ESCAPE)
        {
            listeningAction = -1;
            return;
        }
        TryRebind(listeningAction, key, false,
                  KeybindManager::GetInputDisplayName(key, false),
                  toastLine1, toastLine2, toastTimer);
        listeningAction = -1;
    }
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        TryRebind(listeningAction, MOUSE_BUTTON_LEFT, true, "Mouse Left",
                  toastLine1, toastLine2, toastTimer);
        listeningAction = -1;
    }
    else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        TryRebind(listeningAction, MOUSE_BUTTON_RIGHT, true, "Mouse Right",
                  toastLine1, toastLine2, toastTimer);
        listeningAction = -1;
    }
}

/**
 * @brief Render satu section: header → action entries → info entries
 * @param listeningAction Di-reference karena entry click bisa set nilai baru
 * @param enteredThisFrame Di-set true pas entry di-click
 */
static void RenderSection(const SectionInfo &sec, int startX, int &currentLocalY,
                          int nameColEndX, int keyColX, int mx, int my,
                          int contentStartY, int scrollY,
                          int &listeningAction, bool &enteredThisFrame)
{
    auto screenY = [&](int localY)
    { return contentStartY + localY - scrollY; };
    auto isVisible = [&](int localY, int h) -> bool
    {
        int top = screenY(localY);
        int bottom = top + h;
        return bottom > contentStartY && top < contentStartY + CONTENT_H;
    };

    // Header
    if (isVisible(currentLocalY, HEADER_HEIGHT))
    {
        DrawTextEx(GetOrLoad(FontId::KEYBIND_HEADER), sec.title,
                   Vector2{(float)(startX + COL_X), (float)screenY(currentLocalY)},
                   KEYB_HEADER_SZ, 0, sec.color);
    }
    currentLocalY += HEADER_HEIGHT;

    // Action entries (rebindable)
    for (int ai = 0; ai < sec.actionCount; ai++)
    {
        if (!isVisible(currentLocalY, ROW_HEIGHT))
        {
            currentLocalY += ROW_HEIGHT;
            continue;
        }

        int y = screenY(currentLocalY);
        Action action = static_cast<Action>(sec.actionIndices[ai]);
        int keyBoxX = startX + COL_X;
        int keyBoxY = y;

        const char *actionName = keybindManager.GetActionName(action);

        // DROP_ALL display khusus: "Left Ctrl + [key DROP_ITEM]"
        static char dropAllBuf[64];
        const char *keyName;
        if (action == DROP_ALL)
        {
            const char *dropItemKey = keybindManager.GetKeyDisplayName(DROP_ITEM);
            snprintf(dropAllBuf, sizeof(dropAllBuf), "Left Ctrl + %s", dropItemKey);
            keyName = dropAllBuf;
        }
        else
        {
            keyName = keybindManager.GetKeyDisplayName(action);
        }

        bool isListening = (listeningAction == static_cast<int>(action));
        Color keyColor = isListening ? GREEN : WHITE;

        // Hover background (±1 spasi kiri/kanan)
        Vector2 keySz = MeasureTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), keyName, KEYB_FONT_SZ, 0);
        Vector2 spaceSz = MeasureTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), " ", KEYB_FONT_SZ, 0);
        int rowStartX = keyBoxX - (int)spaceSz.x;
        int rowEndX = keyColX + (int)keySz.x + (int)spaceSz.x;
        int rowW = rowEndX - rowStartX;

        bool hovered = IsInside(mx, my,
                                rowStartX - HITBOX_PAD, keyBoxY - HITBOX_PAD,
                                rowW + HITBOX_PAD * 2, ROW_HEIGHT + HITBOX_PAD * 2);

        Color bgColor = isListening ? Color{40, 80, 40, 255}
                        : hovered   ? Color{50, 50, 50, 255}
                                    : BLANK;
        if (bgColor.a > 0)
            DrawRectangle(rowStartX, keyBoxY, rowW, ROW_HEIGHT, bgColor);

        // Format: "Action Name =>  [Key]" (tab-stop alignment)
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), actionName,
                   Vector2{(float)keyBoxX, (float)y}, KEYB_FONT_SZ, 0, WHITE);
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), " =>  ",
                   Vector2{(float)nameColEndX, (float)y}, KEYB_FONT_SZ, 0, BLACK);
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), keyName,
                   Vector2{(float)keyColX, (float)y}, KEYB_FONT_SZ, 0, keyColor);

        // Click entry → mulai listening
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && listeningAction != static_cast<int>(action))
        {
            listeningAction = static_cast<int>(action);
            enteredThisFrame = true;
        }

        currentLocalY += ROW_HEIGHT;
    }

    // Info entries (combo-only, non-rebindable)
    for (int ii = 0; ii < sec.infoCount; ii++)
    {
        if (!isVisible(currentLocalY, ROW_HEIGHT))
        {
            currentLocalY += ROW_HEIGHT;
            continue;
        }

        int y = screenY(currentLocalY);
        int keyBoxX = startX + COL_X;
        const InfoEntry &info = sec.infoEntries[ii];

        // "Action Name =>  [Combo]" (tab-stop alignment, warna entry biasa)
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), info.actionName,
                   Vector2{(float)keyBoxX, (float)y}, KEYB_FONT_SZ, 0, WHITE);
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), " =>  ",
                   Vector2{(float)nameColEndX, (float)y}, KEYB_FONT_SZ, 0, BLACK);
        DrawTextEx(GetOrLoad(FontId::KEYBIND_ENTRY), info.comboDisplay,
                   Vector2{(float)keyColX, (float)y}, KEYB_FONT_SZ, 0, WHITE);

        currentLocalY += ROW_HEIGHT;
    }

    currentLocalY += ROW_HEIGHT; // spacing antar section
}

/**
 * @brief Toast notifikasi swap di pojok kanan atas (di luar scissor)
 */
static void DrawToastNotification(const char toastLine1[128], const char toastLine2[128], float toastTimer)
{
    if (toastTimer <= 0 || !toastLine1[0])
        return;

    Vector2 sz1 = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), toastLine1, KEYB_TOAST_SZ, 0);
    Vector2 sz2 = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), toastLine2, KEYB_TOAST_SZ, 0);
    int toastW = std::max((int)sz1.x, (int)sz2.x) + 40;
    int toastH = (sz1.y > 0 ? (int)sz1.y + 10 : 0) + (sz2.y > 0 ? (int)sz2.y : 0) + 30;
    int toastX = GScreenWidth - toastW - 20;
    int toastY = 15;

    float alpha = std::min(toastTimer, 1.0f) * 0.85f + 0.15f;
    unsigned char a = static_cast<unsigned char>(alpha * 255.0f);
    DrawRectangle(toastX, toastY, toastW, toastH, Color{20, 20, 30, a});
    DrawRectangleLinesEx(Rectangle{(float)toastX, (float)toastY, (float)toastW, (float)toastH}, 2,
                         Color{255, 165, 0, a});

    float textY = toastY + 15.0f;
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), toastLine1,
               Vector2{(float)(toastX + 20), textY}, KEYB_TOAST_SZ, 0, Color{255, 255, 255, a});
    textY += sz1.y + 10.0f;
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), toastLine2,
               Vector2{(float)(toastX + 20), textY}, KEYB_TOAST_SZ, 0, Color{255, 255, 255, a});
}

/**
 * @brief Overlay "Press a key or click a mouse button. ESC to cancel."
 */
static void DrawListeningPopup(int startX, int startY, int contentW)
{
    const int popupX = startX + (800 - POPUP_W) / 2;
    const int popupY = startY + (600 - POPUP_H) / 2 - 30;

    DrawRectangle(popupX, popupY, POPUP_W, POPUP_H, Color{20, 20, 30, 235});
    DrawRectangleLinesEx(Rectangle{(float)popupX, (float)popupY, (float)POPUP_W, (float)POPUP_H}, 2, GREEN);

    const char *line1 = "Press a key or click a mouse button.";
    const char *line2 = "ESC to cancel.";
    Vector2 sz1 = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), line1, KEYB_TOAST_SZ, 0);
    Vector2 sz2 = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), line2, KEYB_TOAST_SZ, 0);
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), line1,
               Vector2{(float)(popupX + (POPUP_W - sz1.x) / 2), (float)(popupY + 8)},
               KEYB_TOAST_SZ, 0, WHITE);
    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), line2,
               Vector2{(float)(popupX + (POPUP_W - sz2.x) / 2), (float)(popupY + 44)},
               KEYB_TOAST_SZ, 0, GREEN);
}

/** @brief Scrollbar vertikal di pojok kanan konten */
static void DrawScrollbar(int &scrollY, int maxScroll, int contentStartY, int startX, int contentW, int mx, int my)
{
    if (maxScroll <= 0)
        return;

    // Generate thumb texture sekali (white 8x8, di-tint pas render)
    static Texture2D thumbTex = {0};
    static bool texLoaded = false;
    if (!texLoaded)
    {
        Image img = GenImageColor(8, 8, WHITE);
        thumbTex = LoadTextureFromImage(img);
        UnloadImage(img);
        texLoaded = true;
    }

    int barX = startX + 20 + contentW - SCROLLBAR_W;
    int barY = contentStartY;
    int barH = CONTENT_H;

    DrawRectangle(barX, barY, SCROLLBAR_W, barH, Color{40, 40, 40, 255});

    float thumbRatio = (float)CONTENT_H / (float)(maxScroll + CONTENT_H);
    int thumbH = std::max((int)(barH * thumbRatio), 16);
    int maxThumbY = barH - thumbH;
    int thumbY = barY + (scrollY * maxThumbY) / std::max(maxScroll, 1);

    static bool dragging = false;
    static int dragStartMY = 0;
    static int dragStartScrollY = 0;

    bool thumbHovered = IsInside(mx, my, barX, thumbY, SCROLLBAR_W, thumbH);

    if (!dragging)
    {
        if (thumbHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            dragging = true;
            dragStartMY = my;
            dragStartScrollY = scrollY;
        }
    }
    else
    {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            int deltaMY = my - dragStartMY;
            scrollY = dragStartScrollY + (deltaMY * maxScroll) / std::max(maxThumbY, 1);
            scrollY = std::clamp(scrollY, 0, maxScroll);
        }
        else
        {
            dragging = false;
        }
    }

    bool active = dragging || (thumbHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    Color thumbColor = active         ? Color{180, 180, 180, 255}
                       : thumbHovered ? Color{150, 150, 150, 255}
                                      : Color{110, 110, 110, 255};

    DrawTexturePro(thumbTex,
                   Rectangle{0, 0, (float)thumbTex.width, (float)thumbTex.height},
                   Rectangle{(float)barX, (float)thumbY, (float)SCROLLBAR_W, (float)thumbH},
                   Vector2{0, 0}, 0.0f, thumbColor);
}

/*=== Main Entry ===*/

void DrawKeybindsTab(Vector2 mousePosition, int startX, int startY, int contentW)
{
    int mx = static_cast<int>(mousePosition.x);
    int my = static_cast<int>(mousePosition.y);

    static int scrollY = 0;
    static int listeningAction = -1;
    static bool enteredThisFrame = false;
    static char toastLine1[128] = "";
    static char toastLine2[128] = "";
    static float toastTimer = 0.0f;

    if (toastTimer > 0)
        toastTimer -= Time::DELTA_TIME;

    // Total content height
    int totalH = 0;
    for (int si = 0; si < SECTION_COUNT; si++)
    {
        const SectionInfo &sec = sections[si];
        totalH += HEADER_HEIGHT;
        totalH += sec.actionCount * ROW_HEIGHT;
        totalH += sec.infoCount * ROW_HEIGHT;
        totalH += ROW_HEIGHT;
    }

    int contentStartY = startY + CONTENT_TOP;
    int maxScroll = std::max(0, totalH - CONTENT_H);

    scrollY -= static_cast<int>(GetMouseWheelMove()) * ROW_HEIGHT;
    scrollY = std::clamp(scrollY, 0, maxScroll);

    // Tab-stop alignment
    float maxNameW = CalculateMaxNameWidth();
    int nameColEndX = startX + COL_X + (int)maxNameW + NAME_PAD;
    int keyColX = nameColEndX + SEP_GAP;

    // --- Clip ---
    BeginScissorMode(startX + 20, contentStartY, contentW, CONTENT_H);

    // Handle rebind (processes listeningAction from previous frame's click)
    HandleRebindInput(listeningAction, enteredThisFrame, toastLine1, toastLine2, toastTimer);

    // Render all sections
    int currentLocalY = 0;
    for (int si = 0; si < SECTION_COUNT; si++)
    {
        RenderSection(sections[si], startX, currentLocalY,
                      nameColEndX, keyColX, mx, my,
                      contentStartY, scrollY,
                      listeningAction, enteredThisFrame);
    }

    EndScissorMode();

    // Toast (di luar scissor)
    DrawToastNotification(toastLine1, toastLine2, toastTimer);

    // Scrollbar
    DrawScrollbar(scrollY, maxScroll, contentStartY, startX, contentW, mx, my);

    // Listening popup
    if (listeningAction >= 0)
        DrawListeningPopup(startX, startY, contentW);
}
