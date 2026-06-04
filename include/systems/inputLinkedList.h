#pragma once

/** @brief Slot inventory */
enum ItemSlot
{
    SLOT_NONE = 0,      // Tidak ada slot
    SLOT_WEAPON_1 = 1,  // Slot senjata 1
    SLOT_WEAPON_2 = 2,  // Slot senjata 2
    SLOT_POTION_1 = 3,  // Slot potion 1
    SLOT_POTION_2 = 4   // Slot potion 2
};

/** @brief Node untuk linked list slot */
struct SlotNode
{
    ItemSlot slot;    // Nilai slot yang disimpan
    SlotNode* next;   // Pointer ke node berikutnya
    SlotNode* prev;   // Pointer ke node sebelumnya
};

/** @brief Circular linked list untuk hotbar */
class HotbarList
{
public:
    /** @brief Constructor */
    HotbarList();
    /** @brief Destructor */
    ~HotbarList();

    /** @brief Inisialisasi linked list */
    void Initialize();
    /** @brief Ambil slot berikutnya */
    ItemSlot GetNext();
    /** @brief Ambil slot sebelumnya */
    ItemSlot GetPrev();
    /** @brief Set current slot berdasarkan nilai */
    void SetCurrentBySlot(ItemSlot slot);
    /** @brief Ambil slot yang sedang aktif */
    ItemSlot GetCurrentSlot() const { return currentNode ? currentNode->slot : SLOT_NONE; }

private:
    SlotNode* head;        // Pointer ke node pertama
    SlotNode* currentNode; // Pointer ke node aktif
    /** @brief Buat node baru */
    SlotNode* CreateNode(ItemSlot slot);
};
