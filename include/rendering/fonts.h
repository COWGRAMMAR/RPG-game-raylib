#pragma once
#include "raylib.h"

extern Font fontKeybindHeader;   // NewDawn for section headers
extern Font fontKeybindEntry;    // Poppins for keybind entries
extern Font fontLoadingTitle;    // Poppins-Bold for loading screen title

void InitFonts(void);
void UnloadFonts(void);
