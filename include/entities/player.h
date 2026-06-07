#pragma once

#include "../lib/raylib/include/raylib.h"
#include "map.h"
#include "screen.h"
#include "animation.h"
#include "inventory.h"
#include "entity.h"
#include "input.h"
#include "mapLogic.h"

class Player;

/**
 * @file player.h
 * @brief Player System Module
 *
 * Header ini mendeklarasikan kelas Player dan namespace helper
 * untuk movement, combat, interaction, dan inventory.
 */

/**
 * @brief Namespace untuk logika pergerakan pemain dan kamera
 */
namespace Movement
{
    /** @brief Handle input gerak player */
    void HandleMovement(Player &player);
    /** @brief Update kamera mengikuti player */
    void UpdateCamera(Player &player);
    /** @brief Cek apakah player bisa bergerak ke posisi */
    bool CanMove(Player &player, Vector2 newPos);
}

/**
 * @brief Namespace untuk logika pertarungan dan manajemen kesehatan pemain
 */
namespace Combat
{
    /** @brief Update state combat player tiap frame */
    void Update(Player &player);
    /** @brief Handle revive player */
    void HandleRevive(Player &player);
    /** @brief Update animasi swing attack */
    void UpdateSwingAttack(Player &player, float dt);
    /** @brief Render visual swing attack */
    void DrawSwingAttack(Player &player);
}

/**
 * @brief Namespace untuk interaksi lingkungan dan raycasting
 */
namespace Interaction
{
    /** @brief Handle interaksi player */
    void HandleInteractions(Player &player);
    /** @brief Update raycast interaksi */
    void UpdateRaycast(Player &player);
    /** @brief Cek interaksi dengan pintu */
    void CheckDoors(Player &player);
    /** @brief Cek interaksi dengan props */
    void CheckProps(Player &player);
}

/**
 * @brief Namespace untuk hotbar dan penggunaan item
 */
namespace Inventory
{
    /** @brief Handle aksi inventory player */
    void HandleInventoryActions(Player &player);
    /** @brief Gunakan potion dari slot */
    void UsePotion(Player &player, int slotIndex);
}

#include "combat.h"

/**
 * @brief Kelas utama karakter pemain
 *
 * Menangani integrasi input, pergerakan, status pertarungan,
 * dan data interaksi.
 */
class Player : public Entity
{
public:
    Animation Anim;                // Pengontrol animasi
    bool pendingSwitchMap = false; // Flag untuk memicu transisi map
    std::string pendingMapPath;    // Path map tujuan
    std::string pendingDoorName;   // Nama pintu tujuan di map baru
    bool pendingGoBack = false;    // Flag untuk kembali ke map sebelumnya
    GameState *State = nullptr;    // Pointer ke status game global
    bool hasDroppedItems = false;  // Flag cegah double-drop pas mati

    RayCast Ray;                                         // Raycast untuk interaksi
    RayHitResult LastHit;                                // Data tabrakan raycast terakhir
    std::vector<Rectangle> CollisionRects;               // Tile tabrakan yang aktif
    std::vector<std::vector<Vector2>> CollisionPolygons; // Bentuk poligon tabrakan yang aktif
    Vector2 Velocity = {0, 0};                           // Vektor kecepatan gerak saat ini

    /** @brief Inisialisasi player dan lokasi spawn */
    void Init(GameState *state, const char *spawnObjectName = SPAWN_OBJECT_NAME);
    void ResetForNewGame();
    /** @brief Update player tiap frame */
    void Update() override;
    /** @brief Render player */
    void Render(void) override;
    /** @brief Apply damage ke player */
    void TakeDamage(float amount, Vector2 knockback = {0, 0}) override;
    /** @brief Set posisi player */
    void SetPosition(Vector2 pos) { Position = pos; }

    /**
     * @brief Set health dengan pemeriksaan batas
     * @param h Nilai health baru
     */
    void SetHealth(float h)
    {
        Health = h;
        if (Health < 0)
            Health = 0;
        if (Health > MaxHealth)
            Health = MaxHealth;
    }
    /** @brief Set mana player */
    void SetMana(float m) { Mana = m; }
    /** @brief Set item di hotbar */
    void SetHotbarItem(int index, InventoryItem item) { Hotbar[index] = item; }

    /** @brief Draw aim indicator */
    void DrawAimIndicator();

    /** @brief Cek apakah player hidup */
    bool IsAlive() const override { return !Anim.isDead; }

    // --- Getters ---
    /** @brief Get nama player */
    const char *GetName() { return Name; }
    /** @brief Get health player */
    float GetHealth() { return Health; }
    /** @brief Get max health */
    float GetMaxHealth() { return MaxHealth; }
    /** @brief Get mana */
    float GetMana() { return Mana; }
    /** @brief Get max mana */
    float GetMaxMana() { return MaxMana; }
    /** @brief Get sudut raycast */
    float GetRayCastAngle() const { return RayCastAngle; }
    /** @brief Get sudut drop item */
    float GetItemDropAngle() const { return RayCastAngleItemDrop; }
    /** @brief Get sudut drop belakang */
    float GetItemDropAngleBack() const { return RayCastAngleItemDropBack; }
    /** @brief Get range interaksi */
    float GetInteractRange() const { return INTERACT_RANGE; }
    /** @brief Get posisi player */
    Vector2 GetPosition() { return Position; }
    /** @brief Get lebar hitbox */
    float GetHitboxWidth() { return HitboxWidth; }
    /** @brief Get tinggi hitbox */
    float GetHitboxHeight() { return HitboxHeight; }
    /** @brief Get offset X hitbox */
    float GetHitboxOffsetX() { return HitboxOffsetX; }
    /** @brief Get offset Y hitbox */
    float GetHitboxOffsetY() { return HitboxOffsetY; }

    Vector2 GetCenter() const override
    {
        return {
            Position.x + HitboxOffsetX + HitboxWidth / 2,
            Position.y + HitboxOffsetY + HitboxHeight / 2};
    }

    Rectangle GetHitbox() const override
    {
        return {
            Position.x + HitboxOffsetX,
            Position.y + HitboxOffsetY,
            HitboxWidth,
            HitboxHeight};
    }

    /** @brief Get radius magnet pickup */
    float GetMagnetRadius() { return MagnetRadius; }
    /** @brief Get speed item pickup */
    float GetItemSpeed() { return ItemSpeed; }
    /** @brief Get item hotbar */
    InventoryItem &GetHotbarItem(int index) { return Hotbar[index]; }
    /** @brief Get item hotbar (const) */
    const InventoryItem &GetHotbarItem(int index) const { return Hotbar[index]; }
    /** @brief Get item bag */
    InventoryItem &GetBagItem(int index) { return Bag[index]; }
    /** @brief Get item bag (const) */
    const InventoryItem &GetBagItem(int index) const { return Bag[index]; }
    /** @brief Get kapasitas maksimum bag */
    int GetMaxBag() const { return MaxBag; }
    /** @brief Get kapasitas maksimum hotbar */
    int GetMaxHotbar() const { return MaxHotbar; }
    /** @brief Get kapasitas maksimum inventory */
    int GetMaxInventory() const { return MaxInventory; }

    // combat stat
    Combat::Attack attack = {};         // Data status serangan saat ini
    float Mana = 100.0f;                // Poin mana saat ini
    float MaxMana = 100.0f;             // Poin mana maksimum
    float ManaRegenTimer = 0.0f;        // Timer untuk jeda pemulihan mana
    const float ManaRegenDelay = 2.0f;  // Delay sebelum regen mana dimulai
    const float ManaRegenRate = 10.0f;  // Kecepatan regen mana per detik
    const float AttackManaCost = 10.0f; // Biaya mana per serangan

    // movement
    float Speed = 6.0f;          // Kecepatan gerak dasar
    float DashSpeed = 0.0f;       // Current dash speed tambahan
    float DashMaxSpeed = 4.0f;    // Max dash speed
    float DashDecel = 0.06f;      // Lerp factor deselerasi
    float DashCooldown = 0.0f;    // Timer cooldown
    float DashCooldownMax = 0.6f; // Durasi cooldown
    float DashDuration = 0.0f;    // Timer durasi dash aktif
    float DashDurationMax = 0.1f; // Max durasi dash
    float DashManaCost = 8.0f;    // Biaya mana per dash
    bool IsDashing = false;       // Flag sedang dashing
    bool IsMoving = false;        // Flag sedang bergerak
    bool canInteract = false;     // Flag bisa interaksi
    float PotionCooldown = 0.0f;  // Timer cooldown potion
    float PotionCooldownMax = 1.0f; // Durasi cooldown potion

    // Feedback visual/fisika
    float HitFlashTimer = 0.0f;         // Durasi efek kilatan saat terkena hit
    Vector2 KnockbackVelocity = {0, 0}; // Gaya knockback yang sedang diterapkan
    Direction LastHorizDir = RIGHT;     // Arah horizontal terakhir (LEFT/RIGHT)

private:
    const char *Name = "Player Name"; // Nama player
    bool isInitialized = false;       // Flag apakah player sudah di-init

    Rectangle GetPlayerHitboxAtPosition(Vector2 position); // Hitbox player di posisi tertentu
    bool CanMove(Vector2 NewPos);                          // Cek apakah player bisa bergerak ke posisi

    const float INTERACT_RANGE = 32.0f; // Range interaksi player

    // hitbox
    float HitboxWidth = 16.0f;   // Lebar hitbox
    float HitboxHeight = 12.0f;  // Tinggi hitbox
    float HitboxOffsetX = 8.0f;  // Offset X hitbox relatif ke Position
    float HitboxOffsetY = 14.0f; // Offset Y hitbox relatif ke Position

    // inventory sistem
    InventoryItem Hotbar[4];                     // Item akses cepat (hotbar) pemain
    InventoryItem Bag[12];                       // Tas penyimpanan utama pemain
    const int MaxBag = 12;                       // Jumlah maksimum bag
    const int MaxHotbar = 4;                     // Jumlah maksimum hotbar
    const int MaxInventory = MaxBag + MaxHotbar; // Keseluruhan inventory

    // magnet/pickup fields
    float MagnetRadius = 70.0f;        // Default 70
    float ItemSpeed = 300.0f;          // Default 300
    const float RayCastAngle = 0.000f; // cos(90°) — area pandang ±90° dari arah hadap (180 derajat)

    // item drop angle
    const float RayCastAngleItemDrop = 0.000f;    // ±90° dari arah hadap
    const float RayCastAngleItemDropBack = 37.0f; // Zona terlarang belakang (derajat)

    void HandleAction(void); // Handle action input
};

/** @brief Instance global player */
extern Player PlayerInstance;
