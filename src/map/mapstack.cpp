/**
 * @file mapstack.cpp
 * @brief Implementasi dari Map History Stack System
 *
 * File ini berisi implementasi class MapStack yang dideklarasikan di mapstack.h.
 * Dipakai untuk menyimpan riwayat perpindahan antar map agar fitur go back bisa jalan.
 */

#include "mapstack.h"

/*==============================================================================
 * MapSystem Namespace Implementation
 *==============================================================================*/

namespace MapSystem
{
    /*==========================================================================
     * Constructor & Destructor
     *==========================================================================*/

    /**
     * @brief Inisialisasi stack dalam keadaan kosong
     */
    MapStack::MapStack() : top(nullptr), size(0) {}

    /**
     * @brief Bersihkan seluruh node saat object dihancurkan
     */
    MapStack::~MapStack()
    {
        Clear();
    }

    /*==========================================================================
     * Stack Operations
     *==========================================================================*/

    /**
     * @brief Tambahkan riwayat map baru ke posisi teratas stack
     *
     * Entry baru akan menjadi top dan menunjuk ke top sebelumnya.
     *
     * @param mapPath Path map yang dikunjungi
     * @param doorName Nama pintu yang dipakai untuk masuk
     */
    void MapStack::Push(const std::string &mapPath, const std::string &doorName)
    {
        // Bikin node baru
        MapStackNode *newNode = new MapStackNode();
        newNode->data = {mapPath, doorName};

        // Node baru pointing ke top lama
        newNode->next = top;

        // Update top ke node baru
        top = newNode;
        size++;
    }

    /**
     * @brief Hapus dan kembalikan entry paling atas dari stack
     *
     * @return Entry map paling atas, atau entry kosong jika stack kosong
     */
    MapHistoryEntry MapStack::Pop()
    {
        if (IsEmpty())
            return {"", ""}; // return entry kosong kalo stack kosong

        // Simpen node top yang mau dihapus
        MapStackNode *temp = top;
        MapHistoryEntry entry = temp->data;

        // Pindahin top ke node berikutnya
        top = top->next;

        // Hapus node lama
        delete temp;
        size--;

        return entry;
    }

    /**
     * @brief Lihat entry paling atas tanpa menghapusnya dari stack
     *
     * @return Entry map paling atas, atau entry kosong jika stack kosong
     */
    MapHistoryEntry MapStack::Peek() const
    {
        if (IsEmpty())
            return {"", ""}; // return entry kosong kalo stack kosong
        return top->data;
    }

    /**
     * @brief Cek apakah stack sedang kosong
     *
     * @return true jika tidak ada node di stack
     */
    bool MapStack::IsEmpty() const
    {
        return top == nullptr;
    }

    /**
     * @brief Hapus seluruh entry yang ada di stack
     */
    void MapStack::Clear()
    {
        // hapus semua node satu per satu
        while (!IsEmpty())
            Pop();
    }

    /*==========================================================================
     * Serialization Helpers
     *==========================================================================*/

    /**
     * @brief Ambil semua entry dari stack (top-to-bottom)
     *
     * @return Vector berisi entry dari top hingga bottom
     */
    std::vector<MapHistoryEntry> MapStack::GetAllEntries() const
    {
        std::vector<MapHistoryEntry> entries;
        MapStackNode *current = top;
        while (current != nullptr)
        {
            entries.push_back(current->data);
            current = current->next;
        }
        return entries;
    }

    /**
     * @brief Isi stack dari vector (entries[0] menjadi top)
     *
     * Stack akan dikosongkan terlebih dahulu, lalu entry dari vector
     * dimasukkan sehingga entries[0] ada di posisi teratas.
     *
     * @param entries Vector entry yang akan dimasukkan
     */
    void MapStack::FromVector(const std::vector<MapHistoryEntry> &entries)
    {
        Clear();
        for (int i = (int)entries.size() - 1; i >= 0; i--)
        {
            Push(entries[i].mapPath, entries[i].doorName);
        }
    }
}
