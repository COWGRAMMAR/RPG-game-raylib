#pragma once
#include "../lib/raylib/include/raylib.h"

class Player;

/**
 * @brief Logika untuk interaksi pemain dengan lingkungan.
 */
namespace Interaction
{
    /**
     * @brief Titik masuk untuk memproses semua logika interaksi.
     */
    void HandleInteractions(Player &player);

    /** @brief Eksekusi transisi map yang pending */
    void ExecutePendingTransitions(Player &player);

    /**
     * @brief Memperbarui ray interaksi pemain berdasarkan arah hadap.
     */
    void UpdateRaycast(Player &player);

    /**
     * @brief Memeriksa secara spesifik jika ray interaksi mengenai pintu.
     */
    void CheckDoors(Player &player);

    /**
     * @brief Memeriksa jika ray interaksi mengenai properti atau objek.
     */
    void CheckProps(Player &player);
}
