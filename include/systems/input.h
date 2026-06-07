#pragma once

#include "../lib/raylib/include/raylib.h"
#include "inputLinkedList.h"

/**
 * @brief Aksi tingkat tinggi yang dipetakan dari input mentah.
 */
enum PlayerAction
{
    ACTION_NONE,
    ACTION_ATTACK,
    ACTION_DRINK_POTION,
    ACTION_EQUIP_UNEQUIP,
    ACTION_DROP_ITEM,
};

/**
 * @brief Cuplikan (snapshot) status input mentah untuk satu frame.
 */
struct InputState
{
    bool moveUp;
    bool moveDown;
    bool moveLeft;
    bool moveRight;

    bool interact;
    bool toggleInventory;
    bool toggleMap;
    bool leftClickPressed;
    bool rightClickPressed;
    bool leftClickReleased;
    bool rightClickReleased;
    bool leftClickDown;
    bool rightClickDown;
    bool ctrlDown;
    bool goBack;
    bool dropItem;
    bool dropItemAll;

    bool selectSlot1;
    bool selectSlot2;
    bool selectSlot3;
    bool selectSlot4;

    // Absorbed from rogues (were hardcoded in main.cpp / debugmode.cpp)
    bool pauseMenu;
    bool debugToggle;
    bool debugToggleEnemy;
    bool debugTogglePlayer;

    float mouseWheel;
};

/**
 * @brief Kelas manajer untuk mengambil dan menguraikan input pemain.
 * Mengabstraksi status tombol/mouse mentah menjadi aksi dan status khusus game.
 */
class PlayerInput
{
public:
    /** @brief Constructor */
    PlayerInput();

    /**
     * @brief Membaca input mentah dari hardware melalui Raylib.
     */
    void PollInput(void);

    /**
     * @brief Mendapatkan snapshot status input mentah.
     */
    const InputState &GetState() const { return Current; }

    /** @brief Cek apakah player bergerak ke atas */
    bool IsMoveUp() const { return Current.moveUp; }
    /** @brief Cek apakah player bergerak ke bawah */
    bool IsMoveDown() const { return Current.moveDown; }
    /** @brief Cek apakah player bergerak ke kiri */
    bool IsMoveLeft() const { return Current.moveLeft; }
    /** @brief Cek apakah player bergerak ke kanan */
    bool IsMoveRight() const { return Current.moveRight; }
    /** @brief Cek apakah player sedang bergerak */
    bool IsMoving() const { return Current.moveUp || Current.moveDown || Current.moveLeft || Current.moveRight; }
    /** @brief Cek apakah player menekan interact */
    bool IsInteract() const { return Current.interact; }
    /** @brief Cek toggle inventory */
    bool IsToggleInventory() const { return Current.toggleInventory; }
    /** @brief Cek toggle map */
    bool IsToggleMap() const { return Current.toggleMap; }
    /** @brief Cek left click ditekan */
    bool IsLeftClickPressed() const { return Current.leftClickPressed; }
    /** @brief Cek right click ditekan */
    bool IsRightClickPressed() const { return Current.rightClickPressed; }
    /** @brief Cek left click dilepas */
    bool IsLeftClickReleased() const { return Current.leftClickReleased; }
    /** @brief Cek right click dilepas */
    bool IsRightClickReleased() const { return Current.rightClickReleased; }
    /** @brief Cek left click ditahan */
    bool IsLeftClickDown() const { return Current.leftClickDown; }
    /** @brief Cek right click ditahan */
    bool IsRightClickDown() const { return Current.rightClickDown; }
    /** @brief Cek tombol Ctrl ditahan */
    bool IsCtrlDown() { return Current.ctrlDown; }
    /** @brief Cek tombol kembali (back) */
    bool IsGoBack() const { return Current.goBack; }
    /** @brief Cek drop item */
    bool IsDropItem() const { return Current.dropItem; }
    /** @brief Cek drop seluruh item */
    bool IsDropItemAll() const { return Current.dropItemAll; }
    /** @brief Cek select slot 1 */
    bool IsSelectSlot1() const { return Current.selectSlot1; }
    /** @brief Cek select slot 2 */
    bool IsSelectSlot2() const { return Current.selectSlot2; }
    /** @brief Cek select slot 3 */
    bool IsSelectSlot3() const { return Current.selectSlot3; }
    /** @brief Cek select slot 4 */
    bool IsSelectSlot4() const { return Current.selectSlot4; }
    /** @brief Ambil slot aktif */
    ItemSlot GetActiveSlot() const { return ActiveSlot; }
    void SetActiveSlot(ItemSlot slot) { ActiveSlot = slot; }
    bool IsInventoryOpen() const { return InventoryOpen; }
    /** @brief Cek apakah map terbuka */
    bool IsMapOpen() const { return MapOpen; }

    /**
     * @brief Menentukan aksi tingkat tinggi mana yang harus dipicu pada frame ini.
     */
    PlayerAction ResolveAction() const;

    /**
     * @brief Memproses toggle dan logika input persisten.
     */
    void UpdateState(void);

private:
    InputState Current = {};
    ItemSlot ActiveSlot = SLOT_WEAPON_1;
    bool InventoryOpen = false;
    bool MapOpen = false;

    HotbarList doubleLinkedList; // Mengelola perputaran hotbar melalui scroll mouse
};

/** @brief Instance global input */
extern PlayerInput InputInstance;
