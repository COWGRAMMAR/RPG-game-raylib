#include "inv-bst-sort.h"
#include "player.h"
#include <cstring>

namespace Inventory {

BstNode* g_BstRoot = nullptr;

InventoryItem& SlotRef(Player& player, int idx)
{
    if (idx < 4)
        return player.GetHotbarItem(idx);
    return player.GetBagItem(idx - 4);
}

static int CompareSlots(Player& player, int a, int b)
{
    InventoryItem& itemA = SlotRef(player, a);
    InventoryItem& itemB = SlotRef(player, b);

    if (itemA.definitionId == -1 && itemB.definitionId == -1)
        return 0;
    if (itemA.definitionId == -1)
        return 1;
    if (itemB.definitionId == -1)
        return -1;

    const ItemDefinition& defA = itemDefs.GetById(itemA.definitionId);
    const ItemDefinition& defB = itemDefs.GetById(itemB.definitionId);

    int cmp = defA.name.compare(defB.name);
    if (cmp != 0)
        return cmp;

    return a - b;
}

BstNode* BstNewNode(int slotIndex)
{
    return new BstNode(slotIndex);
}

void BstInsert(BstNode*& root, int slotIndex, Player& player)
{
    if (SlotRef(player, slotIndex).definitionId == -1)
        return;

    if (root == nullptr)
    {
        root = BstNewNode(slotIndex);
        return;
    }

    int cmp = CompareSlots(player, slotIndex, root->slotIndex);
    if (cmp < 0)
        BstInsert(root->left, slotIndex, player);
    else if (cmp > 0)
        BstInsert(root->right, slotIndex, player);
}

static BstNode* FindMin(BstNode* node)
{
    while (node && node->left)
        node = node->left;
    return node;
}

static BstNode* RemoveNode(BstNode*& root, int slotIndex, Player& player)
{
    if (root == nullptr)
        return root;

    int cmp = CompareSlots(player, slotIndex, root->slotIndex);
    if (cmp < 0)
    {
        root->left = RemoveNode(root->left, slotIndex, player);
    }
    else if (cmp > 0)
    {
        root->right = RemoveNode(root->right, slotIndex, player);
    }
    else
    {
        BstNode* temp = root;
        if (root->left == nullptr)
        {
            root = root->right;
            delete temp;
        }
        else if (root->right == nullptr)
        {
            root = root->left;
            delete temp;
        }
        else
        {
            BstNode* successor = FindMin(root->right);
            root->slotIndex = successor->slotIndex;
            root->right = RemoveNode(root->right, successor->slotIndex, player);
        }
    }
    return root;
}

void BstRemove(BstNode*& root, int slotIndex, Player& player)
{
    RemoveNode(root, slotIndex, player);
}

void BstClear(BstNode*& root)
{
    if (root == nullptr)
        return;
    BstClear(root->left);
    BstClear(root->right);
    delete root;
    root = nullptr;
}

void BstRebuild(BstNode*& root, Player& player)
{
    BstClear(root);

    for (int i = 0; i < player.GetMaxHotbar(); i++)
    {
        if (player.GetHotbarItem(i).definitionId != -1)
            BstInsert(root, i, player);
    }
    for (int i = 0; i < player.GetMaxBag(); i++)
    {
        int idx = player.GetMaxHotbar() + i;
        if (player.GetBagItem(i).definitionId != -1)
            BstInsert(root, idx, player);
    }
}

void BstTraverse(BstNode* root, void (*visit)(int slotIndex))
{
    if (root == nullptr)
        return;
    BstTraverse(root->left, visit);
    visit(root->slotIndex);
    BstTraverse(root->right, visit);
}

void SortBagWithBst(Player& player)
{
    // Ensure BST is up to date (drag & drop can bypass BST ops)
    BstRebuild(g_BstRoot, player);

    int hotbarCount = player.GetMaxHotbar();
    int bagCount = player.GetMaxBag();

    // In-order traversal to collect bag items in sorted order
    std::vector<InventoryItem> sortedItems;
    std::vector<BstNode*> stack;
    BstNode* curr = g_BstRoot;

    while (curr || !stack.empty())
    {
        while (curr)
        {
            stack.push_back(curr);
            curr = curr->left;
        }
        curr = stack.back();
        stack.pop_back();

        if (curr->slotIndex >= hotbarCount)
            sortedItems.push_back(player.GetBagItem(curr->slotIndex - hotbarCount));

        curr = curr->right;
    }

    if (sortedItems.empty())
        return;

    // Clear all bag slots
    for (int i = 0; i < bagCount; i++)
        player.GetBagItem(i) = {-1, 0};

    // Write items back in BST-sorted order
    int count = std::min((int)sortedItems.size(), bagCount);
    for (int i = 0; i < count; i++)
        player.GetBagItem(i) = sortedItems[i];

    // Rebuild BST with new slot positions
    BstRebuild(g_BstRoot, player);
}

}
