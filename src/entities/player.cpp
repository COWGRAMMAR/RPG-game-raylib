/**
 * @file player.cpp
 * @brief Implementasi Player Character
 *
 * File ini berisi implementasi class Player:
 * - Init: inisialisasi stat, spawn position, collision geometry
 * - Update: input, lifecycle, physics, combat, inventory, interaction, animation
 * - Render: sprite rendering, hit flash, aim indicator
 * - TakeDamage: damage processing, knockback, hit flash
 * - HandleAction: action handler untuk drop item
 */

#include "player.h"
#include "screen.h"
#include "movement.h"
#include "combat.h"
#include "interaction.h"
#include "animation.h"
#include "inventory.h"
#include "mapLogic.h"
#include "game_debug.h"
#include "../lib/raylib/include/raymath.h"
#include "propsbehavior.h"
#include <cmath>

constexpr int EMPTY_ITEM_ID = -1;

/** @brief Instance global player */
Player PlayerInstance;

/**
 * Menginisialisasi karakter pemain.
 * Menangani pemuatan resource satu kali (tekstur) dan inisialisasi per map (spawn/tabrakan).
 */
void Player::Init(GameState *state, const char *spawnObjectName)
{
    State = state;

    // Memuat resource global pemain hanya satu kali
    if (!isInitialized)
    {
        MaxHealth = 100.0f;
        Health = MaxHealth;

        MaxMana = 100.0f;
        Mana = MaxMana;
        ManaRegenTimer = 0.0f;

        // Inisialisasi perlengkapan hotbar default
        Hotbar[0] = {1, 1}; // Iron Sword
        Hotbar[1] = {4, 1}; // Iron Axe
        Hotbar[2] = {2, 8}; // Health Potion
        Hotbar[3] = {3, 8}; // Mana Bread

        for (int i = 0; i < PlayerInstance.MaxBag; i++)
            Bag[i] = {EMPTY_ITEM_ID, 0};

        isInitialized = true;
        TraceLog(LOG_INFO, "Player: Resource global dan statistik telah diinisialisasi");
    }

    Anim.animSet = &loadedAnimationSets["knight"];
    PlayAnimation(Anim, IDLE, RIGHT);
    LastHorizDir = RIGHT;
    CollisionRects.clear();
    CollisionPolygons.clear();

    if (tilesonMap == nullptr)
    {
        Position = {0.0f, 0.0f};
        Anim.position = Position;
        return;
    }

    // Menentukan posisi spawn berdasarkan data map atau fallback ke titik tengah map
    Vector2 spawnPos;
    if (spawnObjectName && spawnObjectName[0] != '\0' && TiledHelperFunction.TryGetObjectPositionByName(spawnObjectName, spawnPos))
    {
        Position = spawnPos;
    }
    else if (TiledHelperFunction.TryGetObjectPositionByName(SPAWN_OBJECT_NAME, spawnPos))
    {
        Position = spawnPos;
    }
    else
    {
        Position = {((float)tilesonMap->width * FRAME_SIZE) / 2.0f, ((float)tilesonMap->height * FRAME_SIZE) / 2.0f};
    }

    Anim.position = Position;

    // Memuat geometri tabrakan statis dari map
    TiledHelper::CollisionResult collision;
    if (TiledHelperFunction.TryGetCollisionByLayerName(COLLISION_LAYER_NAME, collision))
    {
        CollisionRects = collision.rects;
        CollisionPolygons = collision.polygons;
    }
}

/**
 * Loop update utama untuk pemain.
 * Dipanggil oleh Entities::Update() setiap frame.
 *
 * Urutan:
 * 1. Input polling
 * 2. Lifecycle checkup (revive)
 * 3. Timer & Status effects
 * 4. Physics & Knockback
 * 5. Logic modules (Movement, Combat, Inventory, Interaction)
 * 6. Animation & Camera update
 * 7. Map transition handling
 */
void Player::Update()
{
    // 1. Memproses Input
    InputInstance.PollInput();
    InputInstance.UpdateState();

    // 2. Pemeriksaan Lifecycle
    if (InputInstance.IsRevive())
    {
        Combat::HandleRevive(*this);
        return;
    }

    if (Anim.isDead)
        return;

    // 3. Timer & Efek Status
    if (HitFlashTimer > 0)
        HitFlashTimer -= Time::DELTA_TIME;

    // 4. Fisika & Pergerakan (termasuk Knockback)
    float fpsNorm = 60.0f;
    float knockbackFriction = 0.85f;
    if (Vector2Length(KnockbackVelocity) > 0.1f)
    {
        Vector2 nextPos = Vector2Add(Position, Vector2Scale(KnockbackVelocity, Time::DELTA_TIME * fpsNorm));
        if (Movement::CanMove(*this, nextPos))
        {
            Position = nextPos;
        }
        KnockbackVelocity = Vector2Scale(KnockbackVelocity, knockbackFriction);
    }
    else
    {
        KnockbackVelocity = {0, 0};
    }

    // 5. Modul Logika
    if (InputInstance.IsGoBack())
    {
        pendingSwitchMap = false;
        pendingGoBack = true;
    }

    // Pergerakan bisa dilakukan bersamaan dengan animasi serangan
    Movement::HandleMovement(*this);

    Combat::Update(*this);
    if (Anim.isDead)
        return;

    Inventory::HandleInventoryActions(*this);
    Interaction::HandleInteractions(*this);
    HandleAction();

    // 6. Update Animasi & Kamera
    Combat::UpdateSwingAttack(*this, Time::DELTA_TIME);
    Anim.position = Position;
    UpdateAnimation(Anim, Time::DELTA_TIME);
    Movement::UpdateCamera(*this);

    // 7. Handle map transitions
    if (pendingGoBack)
    {
        pendingGoBack = false;
        GoBack();
        return;
    }
    if (pendingSwitchMap)
    {
        pendingSwitchMap = false;
        SwitchMap(pendingMapPath.c_str(), pendingDoorName.c_str());
        return;
    }
}

/**
 * Render sprite player dengan animasi di posisi world saat ini.
 * Dipanggil dari Entities::Render() dengan Y-sorting.
 */
void Player::Render(void)
{
    if (!Anim.isDead)
    {
        DrawAimIndicator();
    }

    // Bayangan (Drop shadow)
    int shadowRx = 10, shadowRy = 4;
    DrawEllipse((int)Position.x + FRAME_SIZE / 2, (int)Position.y + FRAME_SIZE - 1, shadowRx, shadowRy, {0, 0, 0, 80});

    // Terapkan warna kilatan (merah saat terkena hit)
    Color tint = WHITE;
    if (HitFlashTimer > 0)
    {
        tint = RED;
    }

    DrawAnimation(Anim, tint);
    Combat::DrawSwingAttack(*this);
}

/** @brief Apply damage ke player */
void Player::TakeDamage(float amount, Vector2 knockback)
{
    Entity::TakeDamage(amount, knockback);

    float hitFlashDuration = 0.15f;
    float knockbackStrength = 6.0f;
    HitFlashTimer = hitFlashDuration;
    KnockbackVelocity = Vector2Scale(knockback, knockbackStrength);

    if (Anim.isAttacking)
    {
        attack.active = false;
        Anim.isAttacking = false;
        PlayAnimation(Anim, IDLE, Anim.direction);
    }

    Vector2 center = {Position.x + FRAME_SIZE / 2, Position.y + FRAME_SIZE / 2};
    Effects::AddDamage(center, amount);

    TraceLog(LOG_INFO, "PLAYER: Menerima %.1f damage. Sisa HP: %.1f", amount, Health);
}

/*==============================================================================
 * Private Helper Methods
 *==============================================================================*/

// Cek apakah player bisa pindah ke posisi
bool Player::CanMove(Vector2 newPosition)
{
    return Movement::CanMove(*this, newPosition);
}

// Hitbox player di posisi tertentu
Rectangle Player::GetPlayerHitboxAtPosition(Vector2 position)
{
    return {position.x + HitboxOffsetX, position.y + HitboxOffsetY, HitboxWidth, HitboxHeight};
}

// Gambar indicator arah aim player (debug overlay)
void Player::DrawAimIndicator(void)
{
    Vector2 playerCenter = GetCenter();
    Vector2 facingDir = {0, 0};
    switch (Anim.direction)
    {
    case UP:
        facingDir = {0, -1};
        break;
    case DOWN:
        facingDir = {0, 1};
        break;
    case LEFT:
        facingDir = {-1, 0};
        break;
    case RIGHT:
        facingDir = {1, 0};
        break;
    }

    Vector2 mouseWorld = GetScreenToWorld2D(GetVirtualMousePosition(State), camera);
    Vector2 aimDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));
    Vector2 rayEnd = {
        playerCenter.x + aimDir.x * INTERACT_RANGE,
        playerCenter.y + aimDir.y * INTERACT_RANGE};

    float dot = Vector2DotProduct(facingDir, aimDir);
    bool isActive = (dot >= RayCastAngle);
    float alpha = isActive ? 1.0f : 0.5f;

    Color indicatorColor = Fade(WHITE, alpha);
    Color outlineColor = Fade(BLACK, alpha);

    // Gambar outline hitam (lebih tebal)
    float outlineThickness = 3.5f;
    float indicatorThickness = 1.5f;
    DrawLineEx(playerCenter, rayEnd, outlineThickness, outlineColor);
    // Gambar garis utama putih (lebih tipis di atas outline)
    DrawLineEx(playerCenter, rayEnd, indicatorThickness, indicatorColor);

    // Raycast hanya terhadap object layer (garis biru) untuk titik merah
    std::vector<MapObject> debugObstacles;
    if (tilesonMap)
    {
        for (auto &obj : tilesonMap->Objects)
        {
            if (obj.layerName == OBJECT_LAYER_NAME)
            {
                debugObstacles.push_back(obj);
            }
        }
    }
    LastHit = Ray.Cast(playerCenter, aimDir, INTERACT_RANGE, debugObstacles);

    // Titik merah hanya di interseksi dengan garis biru (object layer) ketika mode debug aktif
    if (isDebugMode && LastHit.hit)
    {
        float debugCircleRadius = 4.0f;
        DrawCircleV(LastHit.point, debugCircleRadius, Fade(RED, alpha));
    }
}

/*==============================================================================
 * Action Handlers
 *==============================================================================*/

// Handle action player
void Player::HandleAction(void)
{
    if (InputInstance.IsInventoryOpen())
        return;

    PlayerAction action = InputInstance.ResolveAction();

    switch (action)
    {
    case ACTION_DROP_ITEM:
    {
        int slotIdx = (int)InputInstance.GetActiveSlot() - 1;
        if (slotIdx < 0 || slotIdx >= MaxHotbar)
            break;

        InventoryItem &slot = Hotbar[slotIdx];
        if (slot.definitionId == EMPTY_ITEM_ID)
            break;

        bool dropAll = InputInstance.IsDropItemAll();

        Vector2 playerCenter = GetCenter();
        Vector2 mouseWorld = GetScreenToWorld2D(GetVirtualMousePosition(State), camera);
        Vector2 aimDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));

        // Arah hadap player
        Vector2 facingDir = {0, 0};
        switch (Anim.direction)
        {
        case UP:
            facingDir = {0, -1};
            break;
        case DOWN:
            facingDir = {0, 1};
            break;
        case LEFT:
            facingDir = {-1, 0};
            break;
        case RIGHT:
            facingDir = {1, 0};
            break;
        }

        Vector2 backDir = {-facingDir.x, -facingDir.y};
        float forbiddenThreshold = cosf((90.0f - RayCastAngleItemDropBack) * DEG2RAD);
        Vector2 dropDir = aimDir;

        if (Vector2DotProduct(backDir, aimDir) > forbiddenThreshold)
        {
            float cross = facingDir.x * aimDir.y - facingDir.y * aimDir.x;
            float sign = (cross >= 0) ? 1.0f : -1.0f;
            float clampAngle = (90.0f + RayCastAngleItemDropBack) * DEG2RAD;
            float cosA = cosf(clampAngle * sign);
            float sinA = sinf(clampAngle * sign);
            dropDir = {
                facingDir.x * cosA - facingDir.y * sinA,
                facingDir.x * sinA + facingDir.y * cosA};
        }

        Vector2 dropPos = {
            playerCenter.x + dropDir.x * INTERACT_RANGE,
            playerCenter.y + dropDir.y * INTERACT_RANGE};

        ItemSpawn dropped = itemData.CreateItem(dropPos, slot.definitionId);
        if (dropAll)
        {
            dropped.amount = slot.amount;
            slot = {EMPTY_ITEM_ID, 0};
        }
        else
        {
            dropped.amount = 1;
            slot.amount -= 1;
            if (slot.amount <= 0)
                slot = {EMPTY_ITEM_ID, 0};
        }

        itemData.activeItems.push_back(dropped);
        break;
    }

    case ACTION_NONE:
    default:
        break;
    }
}
