#pragma once
#include "../lib/raylib/include/raylib.h"

/**
 * @file entity.h
 * @brief Base Entity Class
 *
 * Header ini mendeklarasikan kelas dasar Entity untuk semua
 * objek interaktif di game: player, enemy, dan props.
 */

/**
 * @brief Kelas dasar untuk semua objek interaktif di dalam game
 *
 * Menyediakan properti dasar seperti posisi, kesehatan,
 * dan lifecycle (update/render).
 */
class Entity
{
public:
    Vector2 Position = {0, 0}; // Posisi dalam koordinat dunia (world space)
    float Health = 100.0f;     // Poin kesehatan saat ini
    float MaxHealth = 100.0f;  // Batas poin kesehatan maksimum
    bool IsActive = true;      // Flag untuk menentukan apakah entitas harus di-update/render

    /** @brief Virtual destructor */
    virtual ~Entity() {}

    /** @brief Logika update untuk entitas, dipanggil tiap frame */
    virtual void Update() = 0;

    /** @brief Render entitas ke layar, dipanggil tiap frame */
    virtual void Render() = 0;

    /** @brief Dapatkan posisi entitas saat ini */
    Vector2 GetPosition() const { return Position; }

    /** @brief Cek apakah entitas masih hidup (Health > 0) */
    virtual bool IsAlive() const { return Health > 0; }

    /** @brief Dapatkan hitbox tabrakan entitas */
    virtual Rectangle GetHitbox() const { return {Position.x, Position.y, 32, 32}; }

    /** @brief Dapatkan titik tengah (center) dari hitbox entitas */
    virtual Vector2 GetCenter() const
    {
        Rectangle hb = GetHitbox();
        return {hb.x + hb.width / 2.0f, hb.y + hb.height / 2.0f};
    }

    /**
     * @brief Berikan damage ke entitas
     * @param amount Jumlah damage
     * @param knockback Vektor knockback (optional)
     */
    virtual void TakeDamage(float amount, Vector2 knockback = {0, 0})
    {
        (void)knockback;
        Health -= amount;
        if (Health < 0)
            Health = 0;
        if (Health > MaxHealth)
            Health = MaxHealth;
    }
};
