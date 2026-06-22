#pragma once

/**
 * @file savemanager.h
 * @brief Unified Save Manager — GameSnapshot + SaveManager
 *
 * Opsi C refactor: menggantikan 4 sistem save paralel (GameStateSaver,
 * WorldgenIO runtime, per-map checkpoint, cache) dengan satu class
 * SaveManager + satu struct GameSnapshot.
 *
 * GameSnapshot = ALL runtime state (player, enemies, items, props,
 * deadEntities, map, meta) dalam satu struct.
 *
 * SaveManager = serialize/deserialize, file I/O, pre/post-spawn apply.
 */

#include "screen.h"
#include "config/game_constants.h"
#include "player.h"
#include "enemy.h"
#include "item.h"
#include "inventory.h"
#include "mapstack.h"
#include "map.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <nlohmann/json.hpp>

// Reuse existing state structs (will be consolidated here in Step 6)
#include "game_state_saver.h"

/*==============================================================================
 * GameSnapshot — ALL runtime state in one struct
 *==============================================================================*/

/**
 * @brief Snapshot lengkap seluruh state runtime game
 *
 * Satu struct untuk semua domain: player, enemies, items, props,
 * dead entities, map, dan metadata.
 *
 * Dipakai untuk:
 * - Manual/autosave (full snapshot)
 * - Worldgen per-stage (stageIndex >= 0)
 * - Per-map checkpoint (mapPath terisi, stageIndex = -1)
 * - Initial snapshot (restart cache replacement)
 */
struct GameSnapshot {
    /**
     * @brief Save file format version.
     * 1 = initial format
     * 2 = added CRC32 integrity hash in "hash" field (via AtomicWrite)
     */
    static constexpr int SNAPSHOT_VERSION = 2;

    /*=== Player ===*/
    Vector2 playerPosition = {0, 0};
    float playerHealth = DEFAULT_MAX_HEALTH;
    float playerMana = DEFAULT_MAX_MANA;
    float playerMaxHealth = DEFAULT_MAX_HEALTH;
    float playerMaxMana = DEFAULT_MAX_MANA;
    InventoryItem hotbar[HOTBAR_SLOTS] = {};
    InventoryItem bag[BAG_SLOTS] = {};

    /** @brief Snapshot animasi dan input state */
    struct {
        int state = 0;          /**< Animation state */
        int direction = 0;      /**< Direction enum */
        bool isDead = false;    /**< Apakah player mati */
        int activeSlot = 0;     /**< Active hotbar slot */
    } animState;

    float dashCooldown = 0.0f;
    float manaRegenTimer = 0.0f;
    nlohmann::json swingAttack;  /**< Serialized attack state */

    /*=== Enemies & Items ===*/
    std::vector<SavedEnemyState> enemies;
    std::vector<SavedItemState> items;

    /*=== Props ===*/
    std::unordered_set<std::string> chestConsumed;
    std::unordered_set<std::string> bombConsumed;
    std::unordered_set<std::string> crateConsumed;

    /** @brief barrierMap[mapPath] = true kalo barrier udah di-clear */
    std::unordered_map<std::string, bool> barrierMap;

    /*=== Dead Entities ===*/
    std::set<std::string> deadEntities;

    /*=== Map ===*/
    std::string mapPath;
    Vector2 cameraTarget = {0, 0};
    float cameraZoom = 1.0f;
    std::vector<MapSystem::MapHistoryEntry> mapHistory;
    std::string mapDisplayName;

    /*=== Meta ===*/
    int version = -1;  // -1 = unloaded/invalid; SNAPSHOT_VERSION = successfully loaded
    std::string timestamp;
    int slotIndex = -1;     /**< -1 = unassigned */
    int worldgenSlot = -1;  /**< -1 = not worldgen */
    int stageIndex = -1;    /**< -1 = full snapshot, >=0 = worldgen per-stage */
    bool showFPS = false;
};

/*==============================================================================
 * SaveManager — unified persistence orchestrator
 *==============================================================================*/

/**
 * @brief Unified save/load orchestrator
 *
 * Single point of truth untuk semua persistence.
 * Ganti semua panggilan ke SaveGameState / RestoreGameState /
 * WorldgenIO::SaveRuntimeState / cache system
 * dengan method di class ini.
 *
 * Semua method static — tidak ada instance state.
 */
class SaveManager {
public:
    /*=== Capture (read live state → GameSnapshot) ===*/

    /**
     * @brief Capture seluruh state runtime saat ini
     * @return GameSnapshot berisi player, enemies, items, props, deadEntities, map
     */
    static GameSnapshot CaptureSnapshot();

    /**
     * @brief Capture initial snapshot (setelah spawn pertama)
     * @param slot Nomor slot tujuan
     * @return true jika berhasil ditulis
     *
     * Digunakan untuk restart: menggantikan cache system lama.
     * File: saves/slot_N/manual/snapshot_initial.json
     */
    static bool CaptureInitialSnapshot(int slot);

    /*=== Apply (write GameSnapshot → live state) ===*/

    /**
     * @brief Apply pre-spawn state dari snapshot
     * @param snap Snapshot sumber
     *
     * HARUS dipanggil SEBELUM InitAll / SpawnEnemiesFromMap / SpawnObject.
     * - Set consumed positions (chest/bomb/crate) — biar SpawnObject() skip yg udah dikonsumsi
     * - Set barrier state
     *
     * @note Dead entities tidak direstore di sini. Kematian per-instance enemy
     *       menggunakan UUID tracking (RegisterDeathByUUID). ApplyPostSpawn menangani
     *       dead enemies via MapObjectID+Name matching langsung.
     */
    static void ApplyPreSpawn(const GameSnapshot& snap);

    /**
     * @brief Apply post-spawn state dari snapshot
     * @param snap Snapshot sumber
     *
     * HARUS dipanggil SETELAH semua spawn selesai.
     * - Restore player HP/inventory/position/animation/combat
     * - Restore enemies via UUID+MapObjectID matching
     * - Ganti seluruh itemData.activeItems dengan snap.items (source of truth)
     * - Set consumed positions (chest/bomb/crate)
     * - Set barrier state
     * - Restore camera + mapHistory
     */
    static void ApplyPostSpawn(const GameSnapshot& snap);

    /**
     * @brief Apply checkpoint state dari snapshot (partial restore)
     * @param snap Snapshot sumber
     *
     * Untuk checkpoint/cache load (bukan full restore).
     * Restore enemies, items, props (chest/bomb/crate consumed, barrier state).
     * TIDAK merestore player, camera, mapHistory.
     * ApplyPreSpawn() HARUS dipanggil SEBELUM fungsi ini.
     */
    static void ApplyCheckpointData(const GameSnapshot& snap);

    /*=== Full Save/Load ===*/

    /**
     * @brief Tulis snapshot ke file JSON (atomic write)
     * @param snap Snapshot yang akan disimpan
     * @param path Path file tujuan
     * @return true jika sukses
     */
    static bool WriteSnapshot(const GameSnapshot& snap, const std::string& path);

    /**
     * @brief Baca snapshot dari file JSON
     * @param path Path file sumber
     * @return GameSnapshot yang terbaca (kosong jika gagal)
     *
     * Cek snap.version == SNAPSHOT_VERSION untuk validasi.
     */
    static GameSnapshot ReadSnapshot(const std::string& path);

    /**
     * @brief Cek apakah file snapshot ada
     * @param path Path file
     * @return true jika file ada dan tidak kosong
     */
    static bool HasSnapshot(const std::string& path);

    /*=== Convenience ===*/

    /**
     * @brief Simpan manual snapshot untuk slot
     * @param snap Snapshot yang akan disimpan
     * @param slot Nomor slot (0-4)
     * @return true jika sukses
     * File: saves/slot_N/manual/snapshot.json
     */
    static bool SaveManual(const GameSnapshot& snap, int slot);

    /**
     * @brief Load manual snapshot dari slot
     * @param slot Nomor slot (0-4)
     * @return GameSnapshot yang terbaca
     */
    static GameSnapshot LoadManual(int slot);

    /**
     * @brief Ada manual save di slot?
     * @param slot Nomor slot
     * @return true jika ada
     */
    static bool HasManual(int slot);

    /**
     * @brief Autosave ke slot aktif
     * @param slot Nomor slot
     * @return true jika sukses
     *
     * Generate filename dengan timestamp.
     * Prune otomatis ke max 5 file autosave per slot.
     */
    static bool SaveAutosave(int slot);

    /*=== Per-map Checkpoint ===*/

    /**
     * @brief Save checkpoint untuk map tertentu
     * @param snap Snapshot (biasanya hanya berisi enemies + items)
     * @param mapPath Path map sebagai key
     * @param slot Nomor slot
     * @return true jika sukses
     * File: saves/slot_N/checkpoints/{mapHash}.json
     */
    static bool SaveCheckpoint(const GameSnapshot& snap, const std::string& mapPath, int slot);

    /**
     * @brief Load checkpoint untuk map
     * @param mapPath Path map sebagai key
     * @param slot Nomor slot
     * @return GameSnapshot yang terbaca
     */
    static GameSnapshot LoadCheckpoint(const std::string& mapPath, int slot);

    /**
     * @brief Cek apakah ada checkpoint untuk map
     * @param mapPath Path map
     * @param slot Nomor slot
     * @return true jika ada
     */
    static bool HasCheckpoint(const std::string& mapPath, int slot);

    /*=== Initial Snapshot (Restart) ===*/

    /**
     * @brief Save initial snapshot (setelah spawn pertama)
     * @param snap Snapshot awal
     * @param slot Nomor slot (UI slot)
     * @return true jika sukses
     * File: saves/slot_N/manual/snapshot_initial.json
     */
    static bool SaveInitial(const GameSnapshot& snap, int slot);

    /**
     * @brief Load initial snapshot
     * @param slot Nomor slot
     * @return GameSnapshot awal
     */
    static GameSnapshot LoadInitial(int slot);

    /**
     * @brief Cek apakah initial snapshot ada
     * @param slot Nomor slot
     * @return true jika ada
     */
    static bool HasInitial(int slot);

    /**
     * @brief Mirror checkpoint + manual dari slot sumber ke runtime workspace (-1)
     * @param sourceSlot Slot sumber (misal 0, 1, dst)
     * @return true jika sukses
     *
     * Plan mode: runtime workspace harus punya data lengkap kayak slot_N.
     * Dipanggil pas loading save dari slot tertentu.
     */
    static bool MirrorToWorkspace(int sourceSlot);

    /**
     * @brief Hapus semua file checkpoint di slot_-1/checkpoints/
     *
     * Dipanggil pas new game biar folder checkpoint kosong,
     * gak bawa sisa checkpoint dari sesi sebelumnya.
     */
    static void ClearWorkspaceCheckpoints();

    /**
     * @brief Hapus semua file di -1/manual/ (snapshot.json + snapshot_initial.json)
     *
     * Dipanggil pas SaveManual() sebelum regenerate snapshot.
     */
    static void ClearWorkspaceManual();

    /**
     * @brief Hapus semua file di -1/autosave/
     *
     * Dipanggil pas SaveManual() biar autosave lama gak ikut ke slot tujuan.
     */
    static void ClearWorkspaceAutosave();

    /**
     * @brief Copy seluruh workspace (-1) ke slot tujuan
     * @param slot Slot tujuan (0-4)
     *
     * Copy checkpoints/, manual/, autosave/ dari -1 ke slot_N.
     * Dipanggil di akhir SaveManual() setelah semua write ke -1 selesai.
     */
    static void CopyWorkspaceTo(int slot);

    /*=== Serialization ===*/

    /**
     * @brief Serialize GameSnapshot ke JSON
     * @param snap Snapshot sumber
     * @return nlohmann::json object
     */
    static nlohmann::json Serialize(const GameSnapshot& snap);

    /**
     * @brief Deserialize JSON ke GameSnapshot
     * @param root JSON object
     * @return GameSnapshot hasil deserialisasi
     */
    static GameSnapshot Deserialize(const nlohmann::json& root);

    /*=== Path Helpers ===*/

    /** @brief Path base untuk slot: saves/slot_N */
    static std::string GetSlotDir(int slot);

    /** @brief Path manual snapshot: saves/slot_N/manual/snapshot.json */
    static std::string GetManualPath(int slot);

    /** @brief Dir autosave: saves/slot_N/autosave/ */
    static std::string GetAutosaveDir(int slot);

    /** @brief Path initial snapshot: saves/slot_N/manual/snapshot_initial.json */
    static std::string GetInitialPath(int slot);

    /**
     * @brief Path checkpoint untuk map
     * Sanitize mapPath jadi safe filename.
     */
    static std::string GetCheckpointPath(const std::string& mapPath, int slot);

    /*=== Slot Management ===*/

    /**
     * @brief Pastikan direktori slot ada
     * @param slot Nomor slot
     * @return true jika sukses
     */
    static bool EnsureDirs(int slot);

    /**
     * @brief Hapus seluruh data slot
     * @param slotIndex Nomor slot (0-11)
     * @return true jika sukses
     * Sama seperti DeleteSaveSlot() — logic-only, tinggal panggil dari UI.
     */
    static bool DeleteSlot(int slotIndex);

    /**
     * @brief Cek apakah slot punya data (manual/autosave/initial)
     * @param slot Nomor slot
     * @return true jika ada minimal satu file save
     */
    /**
     * @brief Hapus semua file .tmp di saves/
     */
    static void CleanupTmpFiles();

private:
    /** @brief Sanitize map path jadi safe filename */
    static std::string SanitizePath(const std::string& mapPath);

    /** @brief Atomic write helper */
    static bool AtomicWrite(const std::string& path, const nlohmann::json& data);

    /** @brief Create directory if not exists */
    static bool EnsureDir(const std::string& dir);
};
