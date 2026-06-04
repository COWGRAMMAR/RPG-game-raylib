#pragma once

/**
 * @file game_state_saver.h
 * @brief Modul Preservasi State Game
 *
 * Handle save dan restore seluruh state game world:
 * - Player (posisi, health, inventory, mana)
 * - Enemy (posisi, HP, alive/dead status)
 * - Item (posisi, collected/remaining status)
 * - Map state (opened chests, triggered events)
 */

#include "screen.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "map.h"
#include "inventory.h"
#include <vector>
#include <string>

/*==============================================================================
 * Saved State Structures
 *==============================================================================*/

/**
 * @brief Struktur data untuk menyimpan state player
 */
struct SavedPlayerState
{
    Vector2 position;        // Posisi player di world
    float health;            // HP player saat ini
    float mana;              // Mana player saat ini
    InventoryItem hotbar[4]; // Hotbar inventory player
};

/**
 * @brief Struktur data untuk menyimpan state satu enemy
 */
struct SavedEnemyState
{
    Vector2 position;      // Posisi enemy di world
    std::string enemyName; // Nama tipe enemy ("Slime"/"Skeleton"/"Wolf")
    int currentHP;         // HP enemy saat ini
    bool isAlive;          // Status hidup/mati enemy
};

/**
 * @brief Struktur data untuk menyimpan state satu item
 *
 * Hanya menyimpan state yang unik per instance.
 * Data statis item (nama, kategori, rarity, dll) diambil dari
 * ItemDefinitionManager via definitionId.
 */
struct SavedItemState
{
    Vector2 position; // Posisi item di world
    bool isPickedUp;  // Status collected/remaining
    int definitionId; // ID referensi ke ItemDefinition
};

/**
 * @brief Struktur data untuk menyimpan state map (chest, dll)
 */
struct SavedMapState
{
    std::string mapPath;                    // Path map saat ini
    Vector2 cameraTarget;                   // Posisi camera target
    float cameraZoom;                       // Zoom camera
    std::vector<unsigned char> chestOpened; // Status opened/closed tiap chest
};

/*==============================================================================
 * Global Saved State Variables
 *==============================================================================*/

/** @brief State player yang tersimpan */
extern SavedPlayerState savedPlayerState;

/** @brief Daftar state enemy yang tersimpan */
extern std::vector<SavedEnemyState> savedEnemyStates;

/** @brief Daftar state item yang tersimpan */
extern std::vector<SavedItemState> savedItemStates;

/** @brief State map yang tersimpan */
extern SavedMapState savedMapState;

/** @brief Flag menandakan apakah ada state tersimpan */
extern bool hasSavedState;

/*==============================================================================
 * State Save/Restore Functions
 *==============================================================================*/

/**
 * @brief Simpan seluruh state game world
 * @details Dipanggil saat player klik "Return to Menu"
 * @param state Pointer ke GameState
 * @note Menyimpan: player position/health/inventory, enemies, items, map state
 */
void SaveGameState(GameState *state);

/**
 * @brief Kembalikan seluruh state game world
 * @details Dipanggil saat masuk PLAY state dari LOADING
 * @param state Pointer ke GameState
 * @note Restore: player, enemies, items, map state dari data tersimpan
 */
void RestoreGameState(GameState *state);

/**
 * @brief Cek apakah ada state tersimpan
 * @return true jika ada state yang bisa di-restore
 * @note Dipakai untuk menentukan apakah ini new game atau resume
 */
bool HasSavedState(void);

/**
 * @brief Bersihkan state tersimpan
 * @details Untuk new game dari awal (fresh start)
 */
void ClearSavedState(void);
