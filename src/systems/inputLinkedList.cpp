#include "inputLinkedList.h"
#include <iostream>

HotbarList::HotbarList() : head(nullptr), currentNode(nullptr) {}

HotbarList::~HotbarList()
{
    if (head == nullptr)
        return;

    SlotNode *temp = head;
    do
    {
        SlotNode *nextNode = temp->next;
        delete temp;
        temp = nextNode;
    } while (temp != head);
}

void HotbarList::Initialize()
{
    if (head != nullptr)
    {
        SlotNode *temp = head;
        do
        {
            SlotNode *nextNode = temp->next;
            delete temp;
            temp = nextNode;
        } while (temp != head);
        head = nullptr;
    }

    SlotNode *node1 = CreateNode(SLOT_WEAPON_1);
    SlotNode *node2 = CreateNode(SLOT_WEAPON_2);
    SlotNode *node3 = CreateNode(SLOT_POTION_1);
    SlotNode *node4 = CreateNode(SLOT_POTION_2);

    node1->next = node2;
    node1->prev = node4;
    node2->next = node3;
    node2->prev = node1;
    node3->next = node4;
    node3->prev = node2;
    node4->next = node1;
    node4->prev = node3;

    head = node1;
    currentNode = node1;
}

ItemSlot HotbarList::GetNext()
{
    if (currentNode)
    {
        currentNode = currentNode->next;
        return currentNode->slot;
    }
    return SLOT_NONE;
}

ItemSlot HotbarList::GetPrev()
{
    if (currentNode)
    {
        currentNode = currentNode->prev;
        return currentNode->slot;
    }
    return SLOT_NONE;
}

void HotbarList::SetCurrentBySlot(ItemSlot slot)
{
    if (head == nullptr)
        return;

    SlotNode *temp = head;
    do
    {
        if (temp->slot == slot)
        {
            currentNode = temp;
            return;
        }
        temp = temp->next;
    } while (temp != head);
}

SlotNode *HotbarList::CreateNode(ItemSlot slot)
{
    SlotNode *newNode = new SlotNode();
    newNode->slot = slot;
    newNode->next = nullptr;
    newNode->prev = nullptr;
    return newNode;
}
