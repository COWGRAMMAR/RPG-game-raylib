#pragma once

/**
 * @file mapstack.h
 * @brief Map History Stack System
 *
 * Header ini mendeklarasikan stack riwayat perpindahan map
 * yang dipakai untuk fitur kembali ke map sebelumnya.
 */

#include <string>
#include <vector>

/*==============================================================================
 * MapSystem Namespace
 *==============================================================================*/

namespace MapSystem
{
    /*==========================================================================
     * MapHistoryEntry Struct
     *==========================================================================*/

    /**
     * @brief Menyimpan satu riwayat kunjungan map
     */
    struct MapHistoryEntry
    {
        std::string mapPath;  // Path file map yang dikunjungi
        std::string doorName; // Nama pintu atau spawn yang terkait
    };

    /*==========================================================================
     * MapStackNode Struct
     *==========================================================================*/

    /**
     * @brief Node linked list untuk penyimpanan stack
     */
    struct MapStackNode
    {
        MapHistoryEntry data; // Data riwayat map
        MapStackNode *next;   // Pointer ke node berikutnya
    };

    /*==========================================================================
     * MapStack Class
     *==========================================================================*/

    /**
     * @brief Stack untuk menyimpan history perpindahan map
     *
     * Entry paling atas adalah map yang terakhir disimpan.
     */
    class MapStack
    {
    public:
        /** @brief Inisialisasi stack kosong */
        MapStack();

        /** @brief Bersihkan seluruh node saat object dihancurkan */
        ~MapStack();

        /**
         * @brief Tambahkan entry map baru ke stack
         * @param mapPath Path map yang dikunjungi
         * @param doorName Nama pintu atau spawn yang dipakai
         */
        void Push(const std::string &mapPath, const std::string &doorName);

        /**
         * @brief Hapus dan kembalikan entry paling atas
         * @return Entry map paling atas
         */
        MapHistoryEntry Pop();

        /**
         * @brief Lihat entry paling atas tanpa menghapusnya
         * @return Entry map paling atas
         */
        MapHistoryEntry Peek() const;

        /**
         * @brief Cek apakah stack kosong
         * @return true jika stack tidak punya node
         */
        bool IsEmpty() const;

        /**
         * @brief Hapus seluruh isi stack
         */
        void Clear();

        /**
         * @brief Ambil seluruh entry stack (top-to-bottom)
         * @return Vector berisi semua entry, elemen pertama = top
         */
        std::vector<MapHistoryEntry> GetAllEntries() const;

        /**
         * @brief Isi stack dari vector (entries[0] menjadi top)
         * @param entries Vector entry untuk mengisi stack
         */
        void FromVector(const std::vector<MapHistoryEntry> &entries);

    private:
        MapStackNode *top; // Pointer ke node teratas
        int size;          // Jumlah node dalam stack
    };
}
