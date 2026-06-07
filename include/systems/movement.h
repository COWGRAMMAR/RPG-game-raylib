#pragma once
#include "../lib/raylib/include/raylib.h"

class Player;

/**
 * @brief Logika fisika dan pergerakan untuk pemain.
 */
namespace Movement
{
    /**
     * @brief Memproses input pemain dan memperbarui kecepatan/posisi.
     */
    void HandleMovement(Player &player);

    /**
     * @brief Memproses dash pemain, termasuk trigger, durasi, cooldown, dan deselerasi.
     * @param player Referensi player yang state dash-nya diperbarui
     */
    void HandleDash(Player &player);

    /**
     * @brief Memperbarui kamera agar mengikuti pemain dengan mulus.
     */
    void UpdateCamera(Player &player);

    /**
     * @brief Pemeriksaan tabrakan untuk posisi target.
     * @return true jika posisi dapat dilewati dan bersih dari rintangan.
     */
    bool CanMove(Player &player, Vector2 newPos);
}
