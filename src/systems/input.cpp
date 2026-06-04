#include "input.h"
#include "player.h"
#include "item.h"
#include "keybindManager.h"

/** @brief Instance global input */
PlayerInput InputInstance;

PlayerInput::PlayerInput()
{
    doubleLinkedList.Initialize();
}

void PlayerInput::PollInput(void)
{
    if (!IsWindowFocused())
    {
        Current = {};
        return;
    }

    // Movement: primary from keybinds, arrows stay as secondary
    Current.moveUp    = IsKeyDown(keybindManager.GetKeycode(MOVE_UP))    || IsKeyDown(KEY_UP);
    Current.moveDown  = IsKeyDown(keybindManager.GetKeycode(MOVE_DOWN))  || IsKeyDown(KEY_DOWN);
    Current.moveLeft  = IsKeyDown(keybindManager.GetKeycode(MOVE_LEFT))  || IsKeyDown(KEY_LEFT);
    Current.moveRight = IsKeyDown(keybindManager.GetKeycode(MOVE_RIGHT)) || IsKeyDown(KEY_RIGHT);

    // Actions (tap)
    Current.interact        = IsKeyPressed(keybindManager.GetKeycode(INTERACT));
    Current.revive          = IsKeyPressed(keybindManager.GetKeycode(REVIVE));
    Current.toggleInventory = IsKeyPressed(keybindManager.GetKeycode(TOGGLE_INVENTORY));
    Current.toggleMap       = IsKeyPressed(keybindManager.GetKeycode(TOGGLE_MAP));
    Current.dropItem        = IsKeyPressed(keybindManager.GetKeycode(DROP_ITEM));

    // Drop-all uses the DROP_ALL key + right Ctrl as secondary mirror
    int dropAllKey = keybindManager.GetKeycode(DROP_ALL);
    Current.dropItemAll = IsKeyDown(dropAllKey) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Mouse (non-rebindable — always the same physical buttons)
    Current.leftClickPressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Current.rightClickPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    Current.leftClickReleased  = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    Current.rightClickReleased = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
    Current.leftClickDown  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Current.rightClickDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    // Ctrl modifier follows DROP_ALL key + right Ctrl
    Current.ctrlDown = IsKeyDown(dropAllKey) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Hotbar selection
    Current.selectSlot1 = IsKeyPressed(keybindManager.GetKeycode(HOTBAR_SLOT_1));
    Current.selectSlot2 = IsKeyPressed(keybindManager.GetKeycode(HOTBAR_SLOT_2));
    Current.selectSlot3 = IsKeyPressed(keybindManager.GetKeycode(HOTBAR_SLOT_3));
    Current.selectSlot4 = IsKeyPressed(keybindManager.GetKeycode(HOTBAR_SLOT_4));

    Current.testLoseHP = IsKeyPressed(keybindManager.GetKeycode(TEST_LOSE_HP));

    // Rogue keys absorbed into InputState
    Current.pauseMenu         = IsKeyPressed(keybindManager.GetKeycode(PAUSE_MENU));
    Current.debugToggle       = IsKeyPressed(keybindManager.GetKeycode(DEBUG_TOGGLE));
    Current.debugToggleEnemy  = IsKeyPressed(keybindManager.GetKeycode(DEBUG_TOGGLE_ENEMY));
    Current.debugTogglePlayer = IsKeyPressed(keybindManager.GetKeycode(DEBUG_TOGGLE_PLAYER));

    Current.mouseWheel = GetMouseWheelMove();
    Current.goBack = IsKeyPressed(keybindManager.GetKeycode(GO_BACK));
}

void PlayerInput::UpdateState(void)
{
    if (Current.toggleInventory)
    {
        InventoryOpen = !InventoryOpen;
        if (InventoryOpen)
            MapOpen = false;

        TraceLog(LOG_INFO, "INPUT: Inventory %s", InventoryOpen ? "OPENED" : "CLOSED");
    }

    if (Current.toggleMap)
    {
        MapOpen = !MapOpen;
        if (MapOpen)
            InventoryOpen = false;

        TraceLog(LOG_INFO, "INPUT: Map %s", MapOpen ? "OPENED" : "CLOSED");
    }

    if (Current.selectSlot1)
    {
        ActiveSlot = SLOT_WEAPON_1;
        doubleLinkedList.SetCurrentBySlot(SLOT_WEAPON_1);
        TraceLog(LOG_INFO, "INPUT: Selected SLOT 1 (Weapon 1)");
    }
    if (Current.selectSlot2)
    {
        ActiveSlot = SLOT_WEAPON_2;
        doubleLinkedList.SetCurrentBySlot(SLOT_WEAPON_2);
        TraceLog(LOG_INFO, "INPUT: Selected SLOT 2 (Weapon 2)");
    }
    if (Current.selectSlot3)
    {
        ActiveSlot = SLOT_POTION_1;
        doubleLinkedList.SetCurrentBySlot(SLOT_POTION_1);
        TraceLog(LOG_INFO, "INPUT: Selected SLOT 3 (Potion 1)");
    }
    if (Current.selectSlot4)
    {
        ActiveSlot = SLOT_POTION_2;
        doubleLinkedList.SetCurrentBySlot(SLOT_POTION_2);
        TraceLog(LOG_INFO, "INPUT: Selected SLOT 4 (Potion 2)");
    }

    if (Current.mouseWheel != 0)
    {
        if (Current.mouseWheel > 0)
            ActiveSlot = doubleLinkedList.GetPrev();
        else
            ActiveSlot = doubleLinkedList.GetNext();

        TraceLog(LOG_INFO, "INPUT: Mouse Wheel moved, selected slot %d", (int)ActiveSlot);
    }

    if (Current.interact)
    {
        TraceLog(LOG_INFO, "INPUT: Interact (E) pressed");
    }
}

PlayerAction PlayerInput::ResolveAction() const
{

    if (Current.dropItem)
        return ACTION_DROP_ITEM;

    // Ambil item di slot aktif
    int slotIdx = (int)ActiveSlot - 1;
    if (slotIdx < 0 || slotIdx >= 4)
        return ACTION_NONE;

    const InventoryItem &item = PlayerInstance.GetHotbarItem(slotIdx);
    if (item.definitionId == -1)
        return ACTION_NONE;

    const ItemDefinition &def = itemDefs.GetById(item.definitionId);
    if (def.category == ITEM_WEAPON)
        return ACTION_ATTACK;
    if (def.category == ITEM_POTION)
        return ACTION_DRINK_POTION;

    return ACTION_NONE;
}
