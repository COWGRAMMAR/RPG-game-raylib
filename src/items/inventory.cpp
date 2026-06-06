/**
 * @file inventory.cpp
 * @brief Implementasi Inventory Interaction System
 *
 * File ini berisi implementasi fungsi-fungsi inventory:
 * - HasInventorySpace, GetActiveHotbarItem
 * - HandleInventoryActions: left-click → drink potion / equip
 * - UsePotion: konsumsi potion dari hotbar
 * - AddToInventory: tambah item ke hotbar/bag dengan stack logic
 * - SetupAttackStats: setup player attack berdasarkan weapon di hotbar
 * - GetAttackManaCost: ambil mana cost dari weapon aktif
 */

#include "item.h"
#include "inventory.h"
#include "inv-bst-sort.h"
#include "player.h"
#include "input.h"
#include "effects.h"
#include "screen.h"

namespace Inventory
{
    /*==============================================================================
     * Utilitas Inventory
     *==============================================================================*/

    /** @brief Cek apakah inventory player masih punya slot kosong */
    bool HasInventorySpace(const Player &player)
    {
        for (int i = 0; i < player.GetMaxHotbar(); i++)
            if (player.GetHotbarItem(i).definitionId == -1)
                return true;
        for (int i = 0; i < player.GetMaxBag(); i++)
            if (player.GetBagItem(i).definitionId == -1)
                return true;
        return false;
    }

    /** @brief Dapatkan item dari slot hotbar yang aktif */
    InventoryItem GetActiveHotbarItem(const Player &player)
    {
        int idx = (int)InputInstance.GetActiveSlot() - 1;
        if (idx >= 0 && idx < player.GetMaxHotbar())
            return player.GetHotbarItem(idx);
        return {-1, 0};
    }

    /*==============================================================================
     * Input & Aksi Inventory
     *==============================================================================*/

    /** @brief Handle left-click action untuk potion/equip */
    void HandleInventoryActions(Player &player)
    {
        // Cooldown timer tick
        if (player.PotionCooldown > 0.0f)
            player.PotionCooldown -= Time::DELTA_TIME;

        // Inventory terbuka = block semua aksi shortcut
        if (InputInstance.IsInventoryOpen())
            return;

        if (player.IsDashing)
            return;

        if (!InputInstance.IsLeftClickPressed())
            return;

        PlayerAction action = InputInstance.ResolveAction();

        if (action == ACTION_DRINK_POTION)
        {
            int slotIdx = (int)InputInstance.GetActiveSlot() - 1;
            if (slotIdx >= 0 && slotIdx < player.GetMaxHotbar())
                UsePotion(player, slotIdx);
        }
        else if (action == ACTION_EQUIP_UNEQUIP)
        {
            TraceLog(LOG_INFO, "PLAYER: Equip/Unequip from inventory!");
        }
    }

    /*==============================================================================
     * Potion
     *==============================================================================*/

    // Konsumsi potion di slot tertentu, cek overflow, kurangi amount
    void UsePotion(Player &player, int slotIndex)
    {
        InventoryItem &slot = player.GetHotbarItem(slotIndex);
        if (slot.definitionId == -1 || slot.amount <= 0)
        {
            Effects::AddLog("Potion telah habis!");
            return;
        }

        const ItemDefinition &def = itemDefs.GetById(slot.definitionId);
        if (def.category != ITEM_POTION)
            return;

        const PotionData &potion = std::get<PotionData>(def.data);

        // Jangan konsumsi jika stat sudah penuh
        if (potion.isMana && player.Mana >= player.MaxMana)
        {
            Effects::AddLog("Mana sudah penuh!");
            return;
        }
        if (!potion.isMana && player.Health >= player.MaxHealth)
        {
            Effects::AddLog("Health sudah penuh!");
            return;
        }

        if (player.PotionCooldown > 0.0f)
        {
            Effects::AddLog("Potion sedang cooldown!");
            return;
        }

        if (potion.isMana)
            player.Mana = std::min(player.Mana + (float)potion.healValue, player.MaxMana);
        else
            player.Health = std::min(player.Health + (float)potion.healValue, player.MaxHealth);

        slot.amount--;
        if (slot.amount <= 0)
        {
            BstRemove(g_BstRoot, slotIndex, player);
            slot = {-1, 0};
        }

        player.PotionCooldown = player.PotionCooldownMax;
    }

    /*==============================================================================
     * Penambahan Item ke Inventory
     *==============================================================================*/

    /** @brief Tambah item ke inventory dengan priority: active slot → existing stack → empty slot */
    bool AddToInventory(Player &player, const ItemSpawn &item)
    {
        const ItemDefinition &def = itemDefs.GetById(item.definitionId);
        bool isStackable = def.isStackable;
        int maxStack = def.maxStack;
        int remaining = item.amount;
        int activeSlot = (int)InputInstance.GetActiveSlot() - 1;

        // 1. Prioritaskan slot hotbar aktif
        if (activeSlot >= 0 && activeSlot < player.GetMaxHotbar())
        {
            InventoryItem &active = player.GetHotbarItem(activeSlot);
            if (active.definitionId == -1)
            {
                int add = std::min(remaining, maxStack);
                active = {item.definitionId, add};
                BstInsert(g_BstRoot, activeSlot, player);
                remaining -= add;
            }
            else if (isStackable && active.definitionId == item.definitionId && active.amount < maxStack)
            {
                int space = maxStack - active.amount;
                int add = std::min(remaining, space);
                active.amount += add;
                remaining -= add;
            }
        }

        if (remaining <= 0)
            return true;

        // 2. Merge ke stack yang sudah ada di hotbar & bag
        if (isStackable)
        {
            for (int i = 0; i < player.GetMaxHotbar() && remaining > 0; i++)
            {
                if (i == activeSlot)
                    continue;
                if (player.GetHotbarItem(i).definitionId == item.definitionId && player.GetHotbarItem(i).amount < maxStack)
                {
                    int space = maxStack - player.GetHotbarItem(i).amount;
                    int add = std::min(remaining, space);
                    player.GetHotbarItem(i).amount += add;
                    remaining -= add;
                }
            }
            for (int i = 0; i < player.GetMaxBag() && remaining > 0; i++)
            {
                if (player.GetBagItem(i).definitionId == item.definitionId && player.GetBagItem(i).amount < maxStack)
                {
                    int space = maxStack - player.GetBagItem(i).amount;
                    int add = std::min(remaining, space);
                    player.GetBagItem(i).amount += add;
                    remaining -= add;
                }
            }
        }

        if (remaining <= 0)
            return true;

        // 3. Slot kosong hotbar (selain active slot)
        for (int i = 0; i < player.GetMaxHotbar() && remaining > 0; i++)
        {
            if (i == activeSlot)
                continue;
            if (player.GetHotbarItem(i).definitionId == -1)
            {
                int add = std::min(remaining, maxStack);
                player.GetHotbarItem(i) = {item.definitionId, add};
                BstInsert(g_BstRoot, i, player);
                remaining -= add;
            }
        }

        // 4. Slot kosong bag sebagai fallback terakhir
        for (int i = 0; i < player.GetMaxBag() && remaining > 0; i++)
        {
            if (player.GetBagItem(i).definitionId == -1)
            {
                int add = std::min(remaining, maxStack);
                player.GetBagItem(i) = {item.definitionId, add};
                BstInsert(g_BstRoot, player.GetMaxHotbar() + i, player);
                remaining -= add;
            }
        }

        // true jika minimal sebagian item berhasil masuk
        return remaining < item.amount;
    }

    /*==============================================================================
     * Senjata & Serangan
     *==============================================================================*/

    /** @brief Setup player attack stats berdasarkan weapon di hotbar */
    void SetupAttackStats(Player &player, Direction attackFaceDir)
    {
        InventoryItem activeItem = GetActiveHotbarItem(player);
        if (activeItem.definitionId == -1)
            return;

        const ItemDefinition &def = itemDefs.GetById(activeItem.definitionId);
        if (def.category != ITEM_WEAPON)
            return;

        const WeaponData &wpn = std::get<WeaponData>(def.data);
        player.attack.weapon = &wpn;

        // Center offset disesuaikan per arah agar hitbox tidak miring
        switch (attackFaceDir)
        {
        case UP:
            player.attack.center.y -= wpn.centerOffset.y;
            player.attack.center.x += wpn.centerOffset.x;
            break;
        case DOWN:
            player.attack.center.y += wpn.centerOffset.y;
            player.attack.center.x -= wpn.centerOffset.x;
            break;
        case LEFT:
            player.attack.center.x -= wpn.centerOffset.x;
            player.attack.center.y -= wpn.centerOffset.y;
            break;
        case RIGHT:
            player.attack.center.x += wpn.centerOffset.x;
            player.attack.center.y += wpn.centerOffset.y;
            break;
        }
    }

    /** @brief Dapatkan mana cost dari weapon aktif */
    float GetAttackManaCost(const Player &player)
    {
        InventoryItem activeItem = GetActiveHotbarItem(player);
        if (activeItem.definitionId == -1)
            return player.AttackManaCost;

        const ItemDefinition &def = itemDefs.GetById(activeItem.definitionId);
        if (def.category != ITEM_WEAPON)
            return player.AttackManaCost;

        return std::get<WeaponData>(def.data).manaCost;
    }
}
