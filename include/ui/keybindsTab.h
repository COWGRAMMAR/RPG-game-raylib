#pragma once

#include "raylib.h"

void DrawKeybindsTab(Vector2 mousePosition, int startX, int startY, int contentW);
void UnloadKeybindsTextures();
const char* GetKeybindsSettingsPath();
