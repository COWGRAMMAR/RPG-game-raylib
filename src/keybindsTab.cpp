/**
 * @file keybindsTab.cpp
 * @brief Implementasi Keybinds Settings Tab
 * 
 * Handle rendering untuk daftar keybinds.
 */

#include "../include/keybindsTab.h"
#include "../lib/raylib/include/raylib.h"
#include <array>

/**
 * @brief Draw keybinds tab content
 * @param mousePosition Posisi mouse untuk efek hover
 * @param startX Posisi X awal area options
 * @param startY Posisi Y awal area options
 */
void DrawKeybindsTab(
    Vector2 mousePosition,
    int startX,
    int startY)
{
    (void)mousePosition;
    int contentStartY = startY + 100;
    const int fontSize = 20;
    int col1X = startX + 40;
    int col2X = startX + 450;

    static const std::array<const char*, 18> keys = {
        "W / Arrow Up",
        "S / Arrow Down",
        "A / Arrow Left",
        "D / Arrow Right",
        "E",
        "I",
        "M",
        "Mouse Left",
        "1",
        "2",
        "3",
        "4",
        "P",
        "TAB",
        "R",
        "K",
        "B",
        "Scroll"
    };
    
    static const std::array<const char*, 18> actions = {
        "Move Up",
        "Move Down",
        "Move Left",
        "Move Right",
        "Interact",
        "Inventory",
        "Map",
        "Action",
        "Weapon 1",
        "Weapon 2",
        "Potion 1",
        "Potion 2",
        "Pause",
        "Debug",
        "Revive",
        "Damage",
        "Prev Map",
        "Zoom / Hotbar Slot"
    };

    for (int i = 0; i < 9; i++) {
        int rowY = contentStartY + i * 28;
        DrawText(keys[i], col1X, rowY, fontSize, YELLOW);
        DrawText(" => ", col1X + 180, rowY, fontSize, GRAY);
        DrawText(actions[i], col1X + 240, rowY, fontSize, WHITE);
    }
    for (int i = 9; i < 18; i++) {
        int rowY = contentStartY + (i - 9) * 28;
        DrawText(keys[i], col2X, rowY, fontSize, YELLOW);
        DrawText(" => ", col2X + 80, rowY, fontSize, GRAY);
        DrawText(actions[i], col2X + 130, rowY, fontSize, WHITE);
    }
}