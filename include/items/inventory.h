#pragma once
#include "item.h"
#include "animation.h"

/**
 * @file inventory.h
 * @brief Inventory System Module
 *
 * Header ini mendeklarasikan fungsi-fungsi untuk manajemen
 * inventory player: add item, use potion, equip weapon.
 */

class Player;

namespace Inventory
{
    /** @brief Proses input inventory per frame (klik kanan, minum potion, equip). */
    void HandleInventoryActions(Player &player);

    /**
     * @brief Konsumsi potion di slot hotbar tertentu.
     * @param slotIndex Index hotbar (0-based).
     * @note Tidak melakukan apa-apa jika slot kosong, bukan potion, atau stat sudah penuh.
     */
    void UsePotion(Player &player, int slotIndex);

    /**
     * @brief Tambahkan item ke inventory player (hotbar → bag).
     * @return true jika minimal sebagian item berhasil masuk.
     * @note Prioritas: active slot → merge stack → slot kosong hotbar → bag.
     */
    bool AddToInventory(Player &player, const ItemSpawn &item);

    /** @brief Cek apakah masih ada slot kosong di hotbar atau bag. */
    bool HasInventorySpace(const Player &player);

    /** @brief Ambil item di slot hotbar yang sedang aktif. */
    InventoryItem GetActiveHotbarItem(const Player &player);

    /**
     * @brief Set parameter swing attack berdasarkan senjata aktif dan arah hadap.
     * @param attackFaceDir Arah player saat menyerang.
     */
    void SetupAttackStats(Player &player, Direction attackFaceDir);

    /**
     * @brief Ambil mana cost serangan dari senjata aktif.
     * @return Mana cost senjata, atau default AttackManaCost jika tidak ada senjata.
     */
    float GetAttackManaCost(const Player &player);
}