#pragma once

/**
 * @file game_state_saver.h
 * @brief Modul Preservasi State Game
 *
 * Handle save dan restore seluruh state game world:
 * - Player (posisi, health, inventory, mana, dash, mana regen, attack)
 * - Enemy (posisi, HP, alive/dead status, spawn point, regen/cooldown timers, UUID)
 * - Item (posisi, collected/remaining status, stackable amount, UUID)
 * - Map state (opened chests, bomb/crate consumed positions, triggered events)
 */

#include "screen.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "map.h"
#include "inventory.h"
#include "mapstack.h"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

/*==============================================================================
 * Constants
 *==============================================================================*/

static constexpr int SAVE_VERSION = 2;      /**< Current save file format version */

/*==============================================================================
 * Saved State Structures
 *==============================================================================*/

/**
 * @brief Struktur data untuk menyimpan state player
 */
struct SavedPlayerState {
    Vector2 position;           /**< Posisi player di world */
    float health;               /**< HP player saat ini */
    float mana;                 /**< Mana player saat ini */
    float maxHealth;            /**< Max HP player */
    float maxMana;              /**< Max Mana player */
    InventoryItem hotbar[4];    /**< Hotbar inventory player */
    InventoryItem bag[12];      /**< Full bag inventory (12 slots) */

    /**
     * @brief Snapshot animasi dan input state
     */
    struct {
        int state;              /**< Animation state (State enum) */
        int direction;          /**< Animation direction (Direction enum) */
        bool isDead;            /**< Apakah player mati */
        int activeSlot;         /**< Active hotbar slot (ItemSlot enum) */
    } animState;

    float dashCooldown;                  /**< Remaining dash cooldown timer */
    float manaRegenTimer;                /**< Timer delay before mana regen begins */
    nlohmann::json swingAttack;          /**< Serialized attack state: active, timer, duration, raycastAngle, center, pressHeld */
};

/**
 * @brief Struktur data untuk menyimpan state satu enemy
 */
struct SavedEnemyState {
    Vector2 position;           /**< Posisi enemy di world */
    std::string enemyName;       /**< Nama tipe enemy ("Slime"/"Skeleton"/"Wolf") */
    int currentHP;              /**< HP enemy saat ini */
    bool isAlive;               /**< Status hidup/mati enemy */
    float maxHealth;            /**< Max HP enemy */
    int aiState;                /**< AI state (EnemyAIState enum: IDLE/PATROL/CHASE/ATTACK/RETURN) */
    float patrolTargetX;        /**< Target patroli X */
    float patrolTargetY;        /**< Target patroli Y */
    float patrolTimer;          /**< Timer tunggu patroli */
    int mapObjectID;            /**< MapObjectID untuk matching saat restore */
    nlohmann::json spawnPoint;           /**< Spawn position serialized as {x, y} */
    float healthRegenTimer;              /**< Countdown before health regen activates, reset on damage */
    float attackCooldownTimer;           /**< Remaining cooldown time between attacks */
    std::string uuid;                    /**< UUID for cross-save enemy matching */
};

/**
 * @brief Struktur data untuk menyimpan state satu item
 *
 * Hanya menyimpan state yang unik per instance.
 * Data statis item (nama, kategori, rarity, dll) diambil dari
 * ItemDefinitionManager via definitionId.
 */
struct SavedItemState {
    Vector2 position;   /**< Posisi item di world */
    bool isPickedUp;    /**< Status collected/remaining */
    int definitionId;   /**< ID referensi ke ItemDefinition */
    int amount = 1;     /**< Jumlah item untuk stackable items */
    std::string uuid;   /**< UUID untuk matching item saat save/load */
};

/**
 * @brief Struktur data untuk menyimpan state map (chest, bomb, crate, dll)
 */
struct SavedMapState {
    std::string mapPath;                              /**< Path map saat ini */
    Vector2 cameraTarget;                          /**< Posisi camera target */
    float cameraZoom;                            /**< Zoom camera */
    std::vector<std::string> deadEntities;                    /**< Nama entitas yang sudah mati di map ini */
    std::vector<std::string> chestsOpened;                    /**< Posisi chest yang sudah dibuka (encoded string) */
    std::vector<MapSystem::MapHistoryEntry> mapHistory;       /**< Stack riwayat perpindahan map */
    nlohmann::json bombConsumedPositions;       /**< Consumed bomb positions as JSON array of strings (matching std::unordered_set<std::string>) */
    nlohmann::json crateConsumedPositions;      /**< Consumed crate positions as JSON array of strings (matching std::unordered_set<std::string>) */
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
 * @note Simpan: player position/health/inventory/mana/dash/manaRegen/attack, enemies with spawnPoint/timers/UUID, items with amount/UUID, map state with bomb/crate/chest consumption
 */
void SaveGameState(GameState *state);

/**
 * @brief Kembalikan seluruh state game world
 * @details Dipanggil saat masuk PLAY state dari LOADING
 * @param state Pointer ke GameState
 * @note Restore: player, enemies (with spawnPoint/timers/UUID), items (with amount/UUID), map state (chests/bombs/crates)
 */
void RestoreGameState(GameState *state);

/**
 * @brief Restore DeadEntities set from saved map state
 * @details Must be called BEFORE InitAll() / SpawnEnemiesFromMap() to prevent
 *          dead enemies from respawning. Reads from savedMapState.deadEntities
 *          and populates the static DeadEntities set via Entities::SetDeadEntities().
 *          Safe to call even if no save state exists.
 */
void RestoreDeadEntities(void);

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

/**
 * @brief Set or clear the worldgen pending flag
 * @details When true, RestoreDeadEntities() will be skipped in the loading
 *          screen because WorldgenIO::LoadRuntimeState handles dead entity
 *          restoration. Set before worldgen map switches, cleared after.
 */
void SetWorldgenPending(bool pending);

/**
 * @brief Check if a worldgen stage load is pending
 * @return true if the next loading screen should skip RestoreDeadEntities()
 *         (worldgen's LoadRuntimeState will set dead entities instead)
 */
bool IsWorldgenPending(void);

/*==============================================================================
 * File I/O Functions
 *==============================================================================*/

/**
 * @brief Write saved state to JSON file
 * @details Serializes all global saved state structs to a JSON file.
 *          Uses atomic write: writes to .tmp then renames.
 * @param path Path to the save file
 */
bool WriteSaveFile(const std::string& path);

/**
 * @brief Write a timestamped autosave to saves/autosave/ directory
 * @details Calls SaveGameState() then writes to saves/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json.
 *          Each call generates a unique filename so autosaves never overwrite each other.
 *          Creates the autosave directory if it doesn't exist.
 * @return true if successful, false if write failed
 */
bool WriteAutosave(const std::string& filename);

/**
 * @brief Read saved state from JSON file
 * @details Reads and deserializes a JSON save file into the global saved state structs.
 *          Validates version == SAVE_VERSION (currently 2).
 * @param path Path to the save file
 * @return true if successful, false if file not found, parse error, or version mismatch
 */
bool ReadSaveFile(const std::string& path);

/**
 * @brief Check if a save file exists and has content
 * @param path Path to the save file
 * @return true if file exists and has non-zero size
 */
bool HasSaveFile(const std::string& path);

/**
 * @brief Delete a save file if it exists
 * @param path Path to the save file to delete
 */
void DeleteSaveFile(const std::string& path);
