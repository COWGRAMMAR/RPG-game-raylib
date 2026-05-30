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
        Hotbar[0] = {0, 1}; // Iron Sword
        Hotbar[1] = {1, 1}; // Iron Axe
        Hotbar[2] = {2, 8}; // Health Potion
        Hotbar[3] = {3, 8}; // Mana Bread

        for (int i = 0; i < PlayerInstance.MaxBag; i++)
            Bag[i] = {-1, 0};

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
        Position = {((float)tilesonMap->width * 32) / 2.0f, ((float)tilesonMap->height * 32) / 2.0f};
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
    if (Vector2Length(KnockbackVelocity) > 0.1f)
    {
        Vector2 nextPos = Vector2Add(Position, Vector2Scale(KnockbackVelocity, Time::DELTA_TIME * 60.0f));
        if (Movement::CanMove(*this, nextPos))
        {
            Position = nextPos;
        }
        KnockbackVelocity = Vector2Scale(KnockbackVelocity, 0.85f); // Redaman gesekan
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
    DrawEllipse((int)Position.x + 16, (int)Position.y + 31, 10, 4, {0, 0, 0, 80});

    // Terapkan warna kilatan (merah saat terkena hit)
    Color tint = WHITE;
    if (HitFlashTimer > 0)
    {
        tint = RED;
    }

    DrawAnimation(Anim, tint);

    if (!Anim.isDead)
    {
        Combat::DrawSwingAttack(*this);
        
        if (canInteract)
        {
            int fontSize = 10;
            const char* text = "[E] Interact";
            int textW = MeasureText(text, fontSize);
            int x = (int)Position.x + 16 - textW / 2;
            int y = (int)Position.y - 12;
            DrawText(text, x + 1, y + 1, fontSize, ColorAlpha(BLACK, 0.7f));
            DrawText(text, x, y, fontSize, YELLOW);
        }
    }
}

void Player::TakeDamage(float amount, Vector2 knockback)
{
    Entity::TakeDamage(amount, knockback);
    PlaySFX("hurt");

    HitFlashTimer = 0.15f;
    KnockbackVelocity = Vector2Scale(knockback, 6.0f);

    if (Anim.isAttacking)
    {
        attack.active = false;
        Anim.isAttacking = false;
        PlayAnimation(Anim, IDLE, Anim.direction);
    }

    Vector2 center = {Position.x + 16, Position.y + 16};
    Effects::AddDamage(center, amount);

    TraceLog(LOG_INFO, "PLAYER: Menerima %.1f damage. Sisa HP: %.1f", amount, Health);
}

/*==============================================================================
 * Private Helper Methods
 *==============================================================================*/

bool Player::CanMove(Vector2 newPosition)
{
    return Movement::CanMove(*this, newPosition);
}

Rectangle Player::GetPlayerHitboxAtPosition(Vector2 position)
{
    return {position.x + HitboxOffsetX, position.y + HitboxOffsetY, HitboxWidth, HitboxHeight};
}

/**
 * @brief Gambar indicator arah aim player (debug overlay)
 * Warna berubah berdasarkan apakah arah aim valid (dot product dengan facing direction)
 * Definisi: src/player.cpp (file ini)
 */
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
    DrawLineEx(playerCenter, rayEnd, 3.5f, outlineColor);
    // Gambar garis utama putih (lebih tipis di atas outline)
    DrawLineEx(playerCenter, rayEnd, 1.5f, indicatorColor);

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
        DrawCircleV(LastHit.point, 4.0f, Fade(RED, alpha));
    }
}

/*==============================================================================
 * Action Handlers
 *==============================================================================*/

/**
 * Resolve apa yang terjadi saat input aksi (misal: Left Click)
 * dilakukan berdasarkan context:
 * - Inventori terbuka → equip/unequip item
 * - Slot senjata (1/2) → attack
 * - Slot potion (3/4) → minum potion
 * Definisi: src/player.cpp (file ini)
 */
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
        if (slot.definitionId == -1)
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
            slot = {-1, 0};
        }
        else
        {
            dropped.amount = 1;
            slot.amount -= 1;
            if (slot.amount <= 0)
                slot = {-1, 0};
        }

        itemData.activeItems.push_back(dropped);
        break;
    }

    case ACTION_NONE:
    default:
        break;
    }
}
