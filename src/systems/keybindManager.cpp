#include "keybindManager.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

/*==============================================================================
 * Global instance
 *==============================================================================*/

KeybindManager keybindManager;

/*==============================================================================
 * Default keybinds — matching current hardcoded values from input.cpp
 *==============================================================================*/

static constexpr int KEY_LEFT_CTRL   = 341;
static constexpr int KEY_RIGHT_CTRL  = 345;
/* KEY_TAB = 258, KEY_GRAVE = 96, KEY_BACKSLASH = 92, KEY_RIGHT_BRACKET = 93
   are already defined in raylib.h */

const Keybind KeybindManager::defaultBindings[ACTION_COUNT] = {
    /* MOVE_UP             */ {KEY_W,             false},
    /* MOVE_DOWN           */ {KEY_S,             false},
    /* MOVE_LEFT           */ {KEY_A,             false},
    /* MOVE_RIGHT          */ {KEY_D,             false},
    /* INTERACT            */ {KEY_E,             false},
    /* TOGGLE_INVENTORY    */ {KEY_I,             false},
    /* TOGGLE_MAP          */ {KEY_M,             false},
    /* DROP_ITEM           */ {KEY_Q,             false},
    /* DROP_ALL            */ {KEY_LEFT_CTRL,     false},
    /* PRIMARY_ATTACK      */ {MOUSE_BUTTON_LEFT,  true},
    /* DASH_DRINK          */ {MOUSE_BUTTON_RIGHT, true},
    /* HOTBAR_SLOT_1       */ {KEY_ONE,           false},
    /* HOTBAR_SLOT_2       */ {KEY_TWO,           false},
    /* HOTBAR_SLOT_3       */ {KEY_THREE,         false},
    /* HOTBAR_SLOT_4       */ {KEY_FOUR,          false},
    /* REVIVE              */ {KEY_R,             false},
    /* TEST_LOSE_HP        */ {KEY_K,             false},
    /* GO_BACK             */ {KEY_B,             false},
    /* PAUSE_MENU          */ {KEY_GRAVE,         false},
    /* DEBUG_TOGGLE        */ {KEY_TAB,           false},
    /* DEBUG_TOGGLE_ENEMY  */ {KEY_BACKSLASH,     false},
    /* DEBUG_TOGGLE_PLAYER */ {KEY_RIGHT_BRACKET, false},
};

const char* KeybindManager::actionNames[ACTION_COUNT] = {
    "MOVE_UP",
    "MOVE_DOWN",
    "MOVE_LEFT",
    "MOVE_RIGHT",
    "INTERACT",
    "TOGGLE_INVENTORY",
    "TOGGLE_MAP",
    "DROP_ITEM",
    "DROP_ALL",
    "ATTACK",
    "DASH_DRINK",
    "HOTBAR_SLOT_1",
    "HOTBAR_SLOT_2",
    "HOTBAR_SLOT_3",
    "HOTBAR_SLOT_4",
    "REVIVE",
    "TEST_LOSE_HP",
    "GO_BACK",
    "PAUSE_MENU",
    "DEBUG_TOGGLE",
    "DEBUG_TOGGLE_ENEMY",
    "DEBUG_TOGGLE_PLAYER",
};

/*==============================================================================
 * Human-readable action display names
 *==============================================================================*/

static const char* actionDisplayNames[ACTION_COUNT] = {
    "Move Up",
    "Move Down",
    "Move Left",
    "Move Right",
    "Interact",
    "Toggle Inventory",
    "Toggle Map",
    "Drop Item",
    "Drop All",
    "Attack",
    "Dash / Drink",
    "Hotbar Slot 1",
    "Hotbar Slot 2",
    "Hotbar Slot 3",
    "Hotbar Slot 4",
    "Revive",
    "Damage (Debug)",
    "Go Back",
    "Pause Menu",
    "Debug Overlay",
    "Debug Enemies",
    "Debug Player",
};

/*==============================================================================
 * Constructor & initialization
 *==============================================================================*/

KeybindManager::KeybindManager()
{
    InitDefaults();
}

void KeybindManager::InitDefaults()
{
    bindings.clear();
    for (int i = 0; i < ACTION_COUNT; i++)
        bindings[static_cast<Action>(i)] = defaultBindings[i];
}

/*==============================================================================
 * Getters & setters
 *==============================================================================*/

int KeybindManager::GetKeycode(Action action) const
{
    auto it = bindings.find(action);
    if (it != bindings.end())
        return it->second.keyCode;
    return 0;
}

bool KeybindManager::IsMouseAction(Action action) const
{
    auto it = bindings.find(action);
    if (it != bindings.end())
        return it->second.isMouse;
    return false;
}

Keybind KeybindManager::GetKeybind(Action action) const
{
    auto it = bindings.find(action);
    if (it != bindings.end())
        return it->second;
    return {0, false};
}

void KeybindManager::SetKeybind(Action action, int keyCode, bool isMouse)
{
    if (action >= 0 && action < ACTION_COUNT)
        bindings[action] = {keyCode, isMouse};
}

static const char* KeyNameFromCode(int code)
{
    switch (code)
    {
        case KEY_SPACE:          return "Space";
        case KEY_ESCAPE:         return "Escape";
        case KEY_ENTER:          return "Enter";
        case KEY_TAB:            return "Tab";
        case KEY_BACKSPACE:      return "Backspace";
        case KEY_INSERT:         return "Insert";
        case KEY_DELETE:         return "Delete";
        case KEY_RIGHT:          return "Right";
        case KEY_LEFT:           return "Left";
        case KEY_DOWN:           return "Down";
        case KEY_UP:             return "Up";
        case KEY_PAGE_UP:        return "Page Up";
        case KEY_PAGE_DOWN:      return "Page Down";
        case KEY_HOME:           return "Home";
        case KEY_END:            return "End";
        case KEY_CAPS_LOCK:      return "Caps Lock";
        case KEY_SCROLL_LOCK:    return "Scroll Lock";
        case KEY_NUM_LOCK:       return "Num Lock";
        case KEY_PRINT_SCREEN:   return "Print Screen";
        case KEY_PAUSE:          return "Pause";
        case KEY_F1:             return "F1";
        case KEY_F2:             return "F2";
        case KEY_F3:             return "F3";
        case KEY_F4:             return "F4";
        case KEY_F5:             return "F5";
        case KEY_F6:             return "F6";
        case KEY_F7:             return "F7";
        case KEY_F8:             return "F8";
        case KEY_F9:             return "F9";
        case KEY_F10:            return "F10";
        case KEY_F11:            return "F11";
        case KEY_F12:            return "F12";
        case KEY_LEFT_SHIFT:     return "Left Shift";
        case KEY_LEFT_CONTROL:   return "Left Ctrl";
        case KEY_LEFT_ALT:       return "Left Alt";
        case KEY_RIGHT_SHIFT:    return "Right Shift";
        case KEY_RIGHT_CONTROL:  return "Right Ctrl";
        case KEY_RIGHT_ALT:      return "Right Alt";
        case KEY_GRAVE:          return "`";
        case KEY_ONE:            return "1";
        case KEY_TWO:            return "2";
        case KEY_THREE:          return "3";
        case KEY_FOUR:           return "4";
        case KEY_FIVE:           return "5";
        case KEY_SIX:            return "6";
        case KEY_SEVEN:          return "7";
        case KEY_EIGHT:          return "8";
        case KEY_NINE:           return "9";
        case KEY_ZERO:           return "0";
        case KEY_MINUS:          return "-";
        case KEY_EQUAL:          return "=";
        case KEY_BACKSLASH:      return "\\";
        case KEY_LEFT_BRACKET:   return "[";
        case KEY_RIGHT_BRACKET:  return "]";
        case KEY_SEMICOLON:      return ";";
        case KEY_APOSTROPHE:     return "'";
        case KEY_COMMA:          return ",";
        case KEY_PERIOD:         return ".";
        case KEY_SLASH:          return "/";
        default:
            // A-Z range: single letter
            if (code >= KEY_A && code <= KEY_Z)
            {
                static char letter[2] = {};
                letter[0] = static_cast<char>('A' + (code - KEY_A));
                return letter;
            }
            return "?";
    }
}

const char* KeybindManager::GetKeyDisplayName(Action action) const
{
    auto it = bindings.find(action);
    if (it == bindings.end())
        return "?";

    if (it->second.isMouse)
    {
        if (it->second.keyCode == MOUSE_BUTTON_LEFT)
            return "Mouse Left";
        if (it->second.keyCode == MOUSE_BUTTON_RIGHT)
            return "Mouse Right";
        if (it->second.keyCode == MOUSE_BUTTON_MIDDLE)
            return "Mouse Middle";
        return "Mouse";
    }

    return KeyNameFromCode(it->second.keyCode);
}

const char* KeybindManager::GetActionName(Action action) const
{
    if (action >= 0 && action < ACTION_COUNT)
        return actionDisplayNames[action];
    return "Unknown";
}

void KeybindManager::ResetDefaults()
{
    InitDefaults();
}

/*==============================================================================
 * JSON persistence
 *==============================================================================*/

bool KeybindManager::LoadFromFile(const std::string& path)
{
    if (!std::filesystem::exists(path))
    {
        TraceLog(LOG_INFO, "KEYBIND: No settings file at %s, using defaults", path.c_str());
        InitDefaults();
        return false;
    }

    try
    {
        std::ifstream file(path);
        json root = json::parse(file);

        if (!root.contains("version") || !root.contains("bindings"))
        {
            TraceLog(LOG_WARNING, "KEYBIND: Invalid settings file, using defaults");
            InitDefaults();
            return false;
        }

        if (root.at("version").get<int>() != 1)
        {
            TraceLog(LOG_WARNING, "KEYBIND: Version mismatch, using defaults");
            InitDefaults();
            return false;
        }

        const auto& bindingsJson = root.at("bindings");

        // Start with defaults, then overlay saved bindings
        InitDefaults();

        int loaded = 0;
        for (int i = 0; i < ACTION_COUNT; i++)
        {
            Action action = static_cast<Action>(i);
            const std::string& key = actionNames[i];

            if (!bindingsJson.contains(key))
                continue;

            const auto& entry = bindingsJson[key];
            if (!entry.contains("key") || !entry.contains("mouse"))
                continue;

            int code = entry["key"].get<int>();
            bool mouse = entry["mouse"].get<bool>();
            bindings[action] = {code, mouse};
            loaded++;
        }

        TraceLog(LOG_INFO, "KEYBIND: Loaded %d/%d bindings from %s", loaded, ACTION_COUNT, path.c_str());
        return true;
    }
    catch (const json::parse_error&)
    {
        TraceLog(LOG_WARNING, "KEYBIND: Parse error in %s, using defaults", path.c_str());
        InitDefaults();
        return false;
    }
    catch (const json::type_error&)
    {
        TraceLog(LOG_WARNING, "KEYBIND: Type error in %s, using defaults", path.c_str());
        InitDefaults();
        return false;
    }
}

bool KeybindManager::SaveToFile(const std::string& path)
{
    try
    {
        json bindingsJson;
        for (int i = 0; i < ACTION_COUNT; i++)
        {
            Action action = static_cast<Action>(i);
            auto it = bindings.find(action);
            if (it == bindings.end())
                continue;

            json entry;
            entry["key"]   = it->second.keyCode;
            entry["mouse"] = it->second.isMouse;
            bindingsJson[actionNames[i]] = entry;
        }

        json root;
        root["version"]  = 1;
        root["bindings"] = bindingsJson;

        // Ensure directory exists
        std::filesystem::path fsPath(path);
        std::filesystem::create_directories(fsPath.parent_path());

        // Atomic write: .tmp then rename
        std::string tmpPath = path + ".tmp";
        if (std::filesystem::exists(tmpPath))
            std::filesystem::remove(tmpPath);

        {
            std::ofstream file(tmpPath);
            file << root.dump(2);
        }

        std::filesystem::rename(tmpPath, path);

        TraceLog(LOG_INFO, "KEYBIND: Saved %zu bindings to %s", bindings.size(), path.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_WARNING, "KEYBIND: Failed to save to %s: %s", path.c_str(), e.what());
        return false;
    }
}
