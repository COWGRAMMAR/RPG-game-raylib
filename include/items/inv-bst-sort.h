#pragma once
#include "item.h"

class Player;

struct BstNode {
    int slotIndex;   // 0-3 hotbar, 4-15 bag
    BstNode* left;
    BstNode* right;

    BstNode(int idx) : slotIndex(idx), left(nullptr), right(nullptr) {}
};

namespace Inventory {

extern BstNode* g_BstRoot;

InventoryItem& SlotRef(Player& player, int idx);

BstNode* BstNewNode(int slotIndex);
void BstInsert(BstNode*& root, int slotIndex, Player& player);
void BstRemove(BstNode*& root, int slotIndex, Player& player);
void BstClear(BstNode*& root);
void BstRebuild(BstNode*& root, Player& player);
void BstTraverse(BstNode* root, void (*visit)(int slotIndex));

/** @brief Sort bag items alphabetically using the BST. Hotbar stays in place. */
void SortBagWithBst(Player& player);

}
