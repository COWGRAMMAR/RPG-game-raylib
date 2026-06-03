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
 * @brief Namespace untuk logika pergerakan pemain dan kamera.
 */
namespace Movement
{
    void HandleMovement(Player &player);
    void UpdateCamera(Player &player);
    bool CanMove(Player &player, Vector2 newPos);
}

/**
 * @brief Namespace untuk logika pertarungan dan manajemen kesehatan pemain.
 */
namespace Combat
{
    void Update(Player &player);
    void HandleRevive(Player &player);
    void UpdateSwingAttack(Player &player, float dt);
    void DrawSwingAttack(Player &player);
}

/**
 * @brief Namespace untuk interaksi lingkungan dan raycasting.
 */
namespace Interaction
{
    void HandleInteractions(Player &player);
    void UpdateRaycast(Player &player);
    void CheckDoors(Player &player);
    void CheckProps(Player &player);
}

/**
 * @brief Namespace untuk hotbar dan penggunaan item.
 */
namespace Inventory
{
    void HandleInventoryActions(Player &player);
    void UsePotion(Player &player, int slotIndex);
}

#include "combat.h"

/**
 * @brief Kelas utama karakter pemain.
 * Menangani integrasi input, pergerakan, status pertarungan, dan data interaksi.
 */
class Player : public Entity
{
public:
    Animation Anim;                ///< Pengontrol animasi
    bool pendingSwitchMap = false; ///< Flag untuk memicu transisi map
    std::string pendingMapPath;    ///< Path map tujuan
    std::string pendingDoorName;   ///< Nama pintu tujuan di map baru
    bool pendingGoBack = false;    ///< Flag untuk kembali ke map sebelumnya
    GameState *State = nullptr;    ///< Pointer ke status game global

    RayCast Ray;                                         ///< Raycast untuk interaksi
    RayHitResult LastHit;                                ///< Data dari tabrakan raycast terakhir
    std::vector<Rectangle> CollisionRects;               ///< Tile tabrakan yang aktif
    std::vector<std::vector<Vector2>> CollisionPolygons; ///< Bentuk poligon tabrakan yang aktif
    Vector2 Velocity = {0, 0};                           ///< Vektor kecepatan gerak saat ini

    /**
     * @brief Inisialisasi status pemain dan lokasi spawn.
     */
    void Init(GameState *state, const char *spawnObjectName = SPAWN_OBJECT_NAME);
    void Update() override;
    void Render(void) override;
    void TakeDamage(float amount, Vector2 knockback = {0, 0}) override;
    void SetPosition(Vector2 pos) { Position = pos; }

    /**
     * @brief Memperbarui kesehatan dengan pemeriksaan batas.
     */
    void SetHealth(float h)
    {
        Health = h;
        if (Health < 0)
            Health = 0;
        if (Health > MaxHealth)
            Health = MaxHealth;
    }
    void SetMana(float m) { Mana = m; }
    void SetHotbarItem(int index, InventoryItem item) { Hotbar[index] = item; }

    // Logic methods
    void DrawAimIndicator();

    // Getter untuk modul logika eksternal
    bool IsAlive() const override { return !Anim.isDead; }
    const char *GetName() { return Name; }
    float GetHealth() { return Health; }
    float GetMaxHealth() { return MaxHealth; }
    float GetMana() { return Mana; }
    float GetMaxMana() { return MaxMana; }

    // getter for inventory + interaction
    float GetRayCastAngle() const { return RayCastAngle; }
    float GetItemDropAngle() const { return RayCastAngleItemDrop; }
    float GetItemDropAngleBack() const { return RayCastAngleItemDropBack; }
    float GetInteractRange() const { return INTERACT_RANGE; }

    // getter position + movement
    Vector2 GetPosition() { return Position; }

    // getter hitbox
    float GetHitboxWidth() { return HitboxWidth; }
    float GetHitboxHeight() { return HitboxHeight; }
    float GetHitboxOffsetX() { return HitboxOffsetX; }
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

    // pickup getters
    float GetMagnetRadius() { return MagnetRadius; }
    float GetItemSpeed() { return ItemSpeed; }

    // Hotbar management getter
    InventoryItem &GetHotbarItem(int index) { return Hotbar[index]; }
    const InventoryItem &GetHotbarItem(int index) const { return Hotbar[index]; }
    InventoryItem &GetBagItem(int index) { return Bag[index]; }
    const InventoryItem &GetBagItem(int index) const { return Bag[index]; }
    int GetMaxBag() const { return MaxBag; }
    int GetMaxHotbar() const { return MaxHotbar; }
    int GetMaxInventory() const { return MaxInventory; }

    // combat stat
    Combat::Attack attack = {};  ///< Data status serangan saat ini
    float Mana = 100.0f;         ///< Poin mana saat ini
    float MaxMana = 100.0f;      ///< Poin mana maksimum
    float ManaRegenTimer = 0.0f; ///< Timer untuk jeda pemulihan mana
    const float ManaRegenDelay = 0.0f;
    const float ManaRegenRate = 5.0f;
    const float AttackManaCost = 10.0f;

    // movement
    float Speed = 3.5f;           ///< Kecepatan gerak dasar
    float DashSpeed = 0.0f;       // current dash speed tambahan
    float DashMaxSpeed = 4.0f;    // max dash speed
    float DashDecel = 0.06f;      // lerp factor deselerasi
    float DashCooldown = 0.0f;    // timer cooldown
    float DashCooldownMax = 1.0f; // durasi cooldown
    float DashDuration = 0.0f;
    float DashDurationMax = 0.1f;
    float DashManaCost = 5.0f;
    bool IsDashing = false;
    bool IsMoving = false;
    bool canInteract = false;

    // Buff states
    float BuffDamageMultiplier = 1.0f;
    float BuffSpeedMultiplier = 1.0f;
    float BuffDamageTimer = 0.0f;
    float BuffSpeedTimer = 0.0f;
    float InvincibilityTimer = 0.0f;

    // Feedback visual/fisika
    float HitFlashTimer = 0.0f;         ///< Durasi efek kilatan saat terkena hit
    Vector2 KnockbackVelocity = {0, 0}; ///< Gaya dorong balik (knockback) yang sedang diterapkan
    Direction LastHorizDir = RIGHT;     ///< Arah horizontal terakhir pemain (LEFT atau RIGHT)

private:
    const char *Name = "Player Name";
    bool isInitialized = false;

    Rectangle GetPlayerHitboxAtPosition(Vector2 position);
    bool CanMove(Vector2 NewPos);

    const float INTERACT_RANGE = 32.0f;

    // hitbox
    float HitboxWidth = 16.0f;
    float HitboxHeight = 12.0f;
    float HitboxOffsetX = 8.0f;
    float HitboxOffsetY = 14.0f;

    // inventory sistem
    InventoryItem Hotbar[4];                     ///< Item akses cepat (hotbar) pemain
    InventoryItem Bag[12];                       ///< Tas penyimpanan utama pemain (inventory, 5x5 grid)
    const int MaxBag = 12;                       ///< jumlah maksimum bag. nilainya harus sama dengan bag
    const int MaxHotbar = 4;                     ///< jumlah maksimum hotbar. nilainya harus sama dengan hotbar
    const int MaxInventory = MaxBag + MaxHotbar; ///< keseluruhan inventory

    // magnet/pickup fields
    float MagnetRadius = 70.0f;        // default 70
    float ItemSpeed = 300.0f;          // default 300
    const float RayCastAngle = 0.000f; ///< cos(90°) — area pandang ±90° dari arah hadap (180 derajat)

    // item drop angle
    const float RayCastAngleItemDrop = 0.000f;    // ±90° dari arah hadap
    const float RayCastAngleItemDropBack = 37.0f; // zona terlarang belakang (derajat)

    // Action handler — definisi: src/player.cpp
    void HandleAction(void);
};

extern Player PlayerInstance;
