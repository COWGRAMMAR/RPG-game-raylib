#pragma once
#include <string>

#include "raylib.h"

/**
 * @file worldgenio.h
 * @brief World Generation Save/Load Module
 *
 * Header ini mendeklarasikan fungsi-fungsi untuk persistensi
 * world generation: save/load runtime state, meta data, dan
 * manajemen save slot.
 */

/**
 * @brief Fungsi I/O untuk sistem save/load world generation
 *
 * Semua state run game disimpan dalam folder worldseed/save_N/.
 * Tiap stage punya data runtime sendiri di
 * runtime_{g_ActiveSaveSlot}.json (per-UI-save-slot isolasi),
 * dan metadata global ada di meta.json.
 */
namespace WorldgenIO
{
    /**
     * @brief Inisialisasi run baru di slot tertentu
     * @param saveSlot Nomor slot tujuan
     * @return true jika berhasil
     */
    bool InitRun(int saveSlot);

    /** @brief Pindah ke stage berikutnya (increment index) */
    void NextStage();

    /** @brief Kembali ke stage sebelumnya (decrement index) */
    void PrevStage();

    /**
     * @brief Dapatkan path file map untuk stage tertentu
     * @param stageIndex Index stage
     * @return Path lengkap ke file JSON map
     */
    std::string GetStagePath(int stageIndex);

    /**
     * @brief Dapatkan path folder meta untuk slot tertentu
     * @param slot Nomor slot
     * @return Path ke meta.json slot yang diminta
     */
    std::string GetMetaPath(int slot);

    /** @brief Cari slot kosong berikutnya (scan folder save_*) */
    int GetNextAvailableSlot();

    /** @brief Dapatkan nomor slot tertinggi yang tersedia */
    int GetTopSlot();

    /** @brief Hapus semua file cache (.cache) di folder saves/ */
    void ClearCache();

    /**
     * @brief Hapus folder worldseed/save_N/ yang gak dipake save manapun
     *
     * Scan semua manual dan autosave file, baca worldgenSlot dari tiap file.
     * Hapus worldseed/save_N/ kalo N gak ada di referensi save manapun.
     */
    void CleanupOrphanedSlots();
}
