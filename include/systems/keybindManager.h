#pragma once

/**
 * @file keybindManager.h
 * @brief Keybind configuration manager
 *
 * Maps game actions to configurable keyboard/mouse keys with JSON persistence.
 * Saves to saves/settings.json at runtime (per-user, not git-tracked).
 */

#include <string>
#include <unordered_map>

/**
 * @brief All rebindable game actions
 */
enum Action {
    // Movement
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    // Gameplay
    INTERACT,
    TOGGLE_INVENTORY,
    TOGGLE_MAP,
    DROP_ITEM,
    DROP_ALL,
    PRIMARY_ATTACK,
    DASH_DRINK,
    // Hotbar selection
    HOTBAR_SLOT_1,
    HOTBAR_SLOT_2,
    HOTBAR_SLOT_3,
    HOTBAR_SLOT_4,
    // Debug gameplay
    REVIVE,
    TEST_LOSE_HP,
    GO_BACK,
    // Menu
    PAUSE_MENU,
    // Debug overlay
    DEBUG_TOGGLE,
    DEBUG_TOGGLE_ENEMY,
    DEBUG_TOGGLE_PLAYER,

    ACTION_COUNT  ///< Sentinel — must be last
};

/**
 * @brief A single keybind entry
 */
struct Keybind {
    int keyCode;   ///< Raylib constant (KEY_W, MOUSE_BUTTON_LEFT, etc.)
    bool isMouse;  ///< true = mouse button, false = keyboard key
};

/**
 * @brief Manages runtime keybind configuration with JSON persistence
 *
 * Loads from saves/settings.json at startup (falls back to safe defaults).
 * Keybinds can be changed in-game via Options > Keybinds and auto-saved.
 */
class KeybindManager {
public:
    KeybindManager();

    /**
     * @brief Load keybinds from JSON file
     * @param path Path to settings file (e.g. "saves/settings.json")
     * @return true if loaded successfully, false if file missing/corrupt (defaults used)
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save current keybinds to JSON file (atomic write)
     * @param path Path to settings file
     * @return true if write succeeded
     */
    bool SaveToFile(const std::string& path);

    /** @brief Get the raylib key code for an action */
    int GetKeycode(Action action) const;

    /** @brief Check if an action is bound to a mouse button */
    bool IsMouseAction(Action action) const;

    /** @brief Get the full Keybind struct */
    Keybind GetKeybind(Action action) const;

    /** @brief Set a keybind for an action */
    void SetKeybind(Action action, int keyCode, bool isMouse);

    /** @brief Get human-readable display name of the bound key (e.g. "E", "Left Ctrl") */
    const char* GetKeyDisplayName(Action action) const;

    /** @brief Get human-readable action name (e.g. "Interact", "Move Up") */
    const char* GetActionName(Action action) const;

    /** @brief Reset all keybinds to factory defaults */
    void ResetDefaults();

private:
    std::unordered_map<Action, Keybind> bindings;
    void InitDefaults();

    static const char* actionNames[ACTION_COUNT];
    static const Keybind defaultBindings[ACTION_COUNT];
};

/** @brief Global keybind manager instance */
extern KeybindManager keybindManager;
