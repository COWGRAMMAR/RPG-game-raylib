#pragma once
#include <string>

#include "../lib/raylib/include/raylib.h"

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
 * Tiap stage punya data runtime sendiri di runtime.json,
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

    /**
     * @brief Simpan state runtime stage saat ini
     * @param stageIndex Index stage yang akan di-save
     * @return true jika berhasil
     */
    bool SaveRuntimeState(int stageIndex);

    /**
     * @brief Muat state runtime stage tertentu
     * @param stageIndex Index stage yang akan di-load
     * @return true jika berhasil
     */
    bool LoadRuntimeState(int stageIndex);

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
}
