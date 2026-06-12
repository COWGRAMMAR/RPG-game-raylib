#pragma once
#include <cstdint>
#include <string>

/**
 * @file seedmanager.h
 * @brief Seed & Save Slot Management Module
 *
 * Header ini mendeklarasikan SeedManager untuk deterministic
 * world generation per stage, serta manajemen save slot.
 */

/**
 * @brief Manager seed deterministic dan save slot
 *
 * Mengelola SEED_COUNT seeds untuk tiap stage, track stage
 * saat ini, prev stage (buat back navigation), dan slot active.
 */
class SeedManager
{
public:
    static constexpr int SEED_COUNT = 5; // Jumlah total stage per run

    /**
     * @brief Inisialisasi run baru di slot tertentu
     * @param saveSlot Nomor slot tujuan
     */
    void InitRun(int saveSlot);

    /**
     * @brief Ambil seed untuk stage tertentu
     * @param stage Index stage
     * @return Seed 32-bit untuk stage tersebut
     */
    uint32_t GetSeed(int stage) const;

    /** @brief Ambil index stage saat ini */
    int GetCurrentStage() const { return currentStage; }

    /** @brief Set index stage saat ini */
    void SetCurrentStage(int stage) { currentStage = stage; }

    /** @brief Ambil nomor slot aktif */
    int GetCurrentSlot() const { return currentSlot; }

    /** @brief Set nomor slot aktif */
    void SetCurrentSlot(int slot) { currentSlot = slot; }

    /** @brief Maju ke stage berikutnya (simpan prev stage) */
    void NextStage()
    {
        if (currentStage < SEED_COUNT - 1)
        {
            prevStage = currentStage;
            currentStage++;
        }
    }

    /** @brief Cek apakah bisa kembali ke stage sebelumnya */
    bool CanGoBack() const { return prevStage >= 0; }

    /** @brief Kembali ke prev stage dan reset prevStage */
    int GoBackStage()
    {
        int s = prevStage;
        prevStage = -1;
        return s;
    }

    /** @brief Cek apakah run sedang aktif */
    bool IsRunActive() const { return isRunActive; }

    /** @brief Reset run (set currentStage ke 0, nonaktifkan) */
    void ResetRun()
    {
        isRunActive = false;
        currentStage = 0;
        prevStage = -1;
    }

    /**
     * @brief Simpan metadata run ke file
     * @param filePath Path file meta.json
     */
    void SaveMeta(const std::string &filePath);

    /**
     * @brief Muat metadata run dari file
     * @param filePath Path file meta.json
     * @return true jika berhasil
     */
    bool LoadMeta(const std::string &filePath);

private:
    uint32_t seeds[SEED_COUNT] = {}; // Array seed deterministic per stage
    int currentStage = 0;            // Index stage saat ini
    int prevStage = -1;              // Index stage sebelumnya (buat back)
    int currentSlot = 1;             // Nomor save slot aktif
    bool isRunActive = false;        // Flag apakah run sedang berlangsung
};

/** @brief Instance global SeedManager */
extern SeedManager g_SeedManager;
