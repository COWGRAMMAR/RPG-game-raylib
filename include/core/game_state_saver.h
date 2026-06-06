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

static constexpr int SAVE_VERSION = 3;      /**< Current save file format version */

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

    int slotIndex = -1;                  /**< Save slot index (0-4 manual, -1 unassigned) */
    std::string saveType = "manual";     /**< "manual" or "autosave" */
    float playTime = 0.0f;               /**< Placeholder play time (tracking deferred) */
    std::string mapDisplayName;          /**< Human-readable map name for UI preview */
    int worldgenSlot = -1;               /**< Worldgen slot mapping (-1 = unassigned) */
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
 * Active Slot Tracking
 *==============================================================================*/

/** @brief Slot save yang sedang aktif (-1 = tidak ada) */
extern int g_ActiveSaveSlot;

/** @brief Flag apakah ada slot yang sedang aktif */
extern bool g_SaveSlotActive;

/**
 * @brief Set slot save yang aktif.
 * @param slot Nomor slot (0-4 valid, -1 = inactive)
 * @note Dipanggil saat: manual save, manual load, autosave, new game.
 *       Reset ke -1 saat kembali ke main menu.
 */
void SetActiveSlot(int slot);

/**
 * @brief Dapatkan slot save yang aktif.
 * @return Nomor slot aktif, -1 jika tidak ada
 */
int GetActiveSlot(void);

/**
 * @brief Cek apakah ada slot yang sedang aktif.
 * @return true jika ada slot aktif
 */
bool IsSlotActive(void);

/**
 * @brief Dapatkan path file save untuk slot dan tipe tertentu.
 * @param slot Nomor slot (0-4)
 * @param type Tipe save ("manual" atau "autosave")
 * @return Path lengkap file save (contoh: "saves/slot_2/manual/manual.json")
 * @note Untuk tipe "manual", return path ke file manual.json.
 *       Untuk tipe "autosave", return path ke direktori autosave (tanpa nama file).
 */
std::string GetSlotPath(int slot, const std::string& type);

/*==============================================================================
 * Slot Directory Utilities
 *==============================================================================*/

/**
 * @brief Pastikan direktori slot save tersedia.
 * @param slot Nomor slot (0-4)
 * @details Membuat struktur direktori:
 *          - saves/slot_N/manual/
 *          - saves/slot_N/autosave/
 *          - saves/slot_N/enemies/
 *          - saves/slot_N/items/
 *          Tidak melakukan apa-apa jika direktori sudah ada.
 */
void EnsureSlotDirectory(int slot);

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
 * @brief Reset memory state only, does NOT clear worldseed directories
 * @details Reset semua state yang tersimpan di memory untuk fresh start.
 *          Tidak menghapus folder worldseed/save_* — gunakan ResetWorldseed() untuk itu.
 */
void ResetMemoryState(void);

/**
 * @brief Hapus worldseed save_N untuk slot tertentu
 * @param slotIndex Nomor slot yang akan dibersihkan worldseednya
 * @details Menghapus folder assets/maps/World_generation/worldseed/save_{slotIndex}
 *          jika ada. Cocok dipanggil saat New Game confirm untuk membersihkan
 *          worldseed slot tertentu tanpa menyentuh state memory.
 */
void ResetWorldseed(int slotIndex);

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
 * @brief Write a timestamped autosave to per-slot autosave directory
 * @details Calls SaveGameState() then writes to saves/slot_N/autosave/autosave_DD-MM-YYYY-HH-MM-SS.json.
 *          Each call generates a unique filename so autosaves never overwrite each other.
 *          Creates the slot directory if it doesn't exist.
 *          After write, prunes to keep only the 5 newest autosave files per slot.
 * @return true if successful, false if write failed or no active slot
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

/*==============================================================================
 * Migration v2 -> v3
 *==============================================================================*/

/**
 * @brief Periksa apakah migrasi v2->v3 diperlukan.
 * @return true jika file saves/manual/slot0.json ada DAN sentinel
 *         saves/.migration_completed_v3 tidak ada.
 * @details Sentinel mencegah migrasi ulang pada launch berikutnya.
 *          Panggil sekali saat startup sebelum menyimpan.
 */
bool NeedsMigration(void);

/**
 * @brief Jalankan pipeline migrasi v2->v3.
 * @return true jika semua langkah berhasil, false jika ada yang gagal.
 * @details Urutan:
 *          1. Copy saves/manual/slot0.json -> saves/slot_0/manual/manual.json (v2->v3 upgrade)
 *          2. Pindahkan saves/enemies/ -> saves/slot_0/enemies/
 *          3. Pindahkan saves/items/ -> saves/slot_0/items/
 *          4. Hapus direktori lama + tulis sentinel
 *          Jika langkah manapun gagal, abort tanpa cleanup.
 */
bool RunMigration(void);

/**
 * @brief Tandai migrasi sebagai selesai dengan menulis sentinel.
 * @details Membuat file kosong saves/.migration_completed_v3.
 *          Sentinel dicek oleh NeedsMigration() untuk skip migrasi.
 */
void MarkMigrationComplete(void);
