#include "combat.h"
#include "screen.h"
#include "animation.h"
#include "entities.h"
#include "input.h"
#include "inventory.h"
#include "map.h"
#include "player.h"
#include "effects.h"
#include "propsbehavior.h"
#include "../lib/raylib/include/raymath.h"
#include "../../include/systems/audioManager.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "mapLogic.h"
#include "propsbehavior.h"
#include "item.h"

namespace Combat
{
    void Update(Player &player)
    {
        HandleDead(player);
        if (player.Anim.isDead)
            return;

        if (InputInstance.IsInventoryOpen())
            return;

        HandleStamina(player);
        HandleAttack(player);
    }

    void HandleDead(Player &player)
    {
        if (player.Health <= 0)
        {
            player.Health = 0;
            if (!player.hasDroppedItems)
                DropAllItems(player);
            PlayAnimation(player.Anim, DEAD, player.Anim.direction);
            player.Anim.isDead = true;
        }
    }

    void HandleStamina(Player &player)
    {
        if (player.ManaRegenTimer > 0)
        {
            player.ManaRegenTimer -= Time::DELTA_TIME;
        }
        else
        {
            if (player.Mana < player.MaxMana)
            {
                player.Mana += player.ManaRegenRate * Time::DELTA_TIME;
                if (player.Mana > player.MaxMana)
                    player.Mana = player.MaxMana;
            }
        }
    }

    void HandleAttack(Player &player)
    {
        if (InputInstance.IsLeftClickPressed())
        {
            player.attack.pressHeld = true;
        }
        if (!InputInstance.IsLeftClickDown())
        {
            player.attack.pressHeld = false;
        }

        if (player.attack.pressHeld && !player.Anim.isAttacking)
        {
            PlayerAction action = InputInstance.ResolveAction();
            if (action == ACTION_ATTACK)
            {
                Vector2 mouseWorld = GetScreenToWorld2D(GetVirtualMousePosition(player.State), camera);
                Vector2 playerCenter = {
                    player.Position.x + player.GetHitboxOffsetX() + player.GetHitboxWidth() / 2,
                    player.Position.y + player.GetHitboxOffsetY() + player.GetHitboxHeight() / 2};
                Vector2 attackDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));

                float angle = atan2(attackDir.y, attackDir.x) * (180.0f / PI);
                Direction attackFaceDir;
                if (angle >= -135.0f && angle < -45.0f)
                    attackFaceDir = UP;
                else if (angle >= -45.0f && angle < 45.0f)
                    attackFaceDir = RIGHT;
                else if (angle >= 45.0f && angle < 135.0f)
                    attackFaceDir = DOWN;
                else
                    attackFaceDir = LEFT;

                Direction horizDir = (attackDir.x >= 0) ? RIGHT : LEFT;
                player.LastHorizDir = horizDir;
                PlayAnimation(player.Anim, IDLE, horizDir);
                PlayAnimation(player.Anim, IDLE, attackFaceDir);

                float manaCost = Inventory::GetAttackManaCost(player);

                if (player.Mana >= manaCost)
                {
                    InventoryItem activeItem = Inventory::GetActiveHotbarItem(player);
                    if (activeItem.definitionId == -1 || itemDefs.GetById(activeItem.definitionId).category != ITEM_WEAPON)
                    {
                        Effects::AddLog("Tidak ada senjata!");
                        player.attack.pressHeld = false;
                        return;
                    }

                    player.Mana -= manaCost;
                    player.ManaRegenTimer = player.ManaRegenDelay;

                    player.attack.active = true;
                    player.attack.timer = 0;
                    player.attack.damagedEntities.clear();
                    player.attack.center = playerCenter;
                    player.attack.raycastAngle = angle;

                    Inventory::SetupAttackStats(player, attackFaceDir);

                    player.Anim.isAttacking = true;
                    AudioManager::PlaySfx();
                    TraceLog(LOG_INFO, "PLAYER: Serangan diarahkan ke (%.2f, %.2f)", attackDir.x, attackDir.y);

                    if (player.attack.weapon->attackType == ATTACK_PIERCE)
                    {
                        Arrow* arrow = new Arrow(playerCenter, attackDir, 300.0f, player.attack.weapon->damage, player.attack.weapon->reach, angle, &player);
                        Entities::AddDynamic(arrow);
                    }
                }
                else
                {
                    Effects::AddLog("Stamina Tidak Cukup");
                    player.attack.pressHeld = false;
                    TraceLog(LOG_WARNING, "PLAYER: Serangan gagal! Mana habis.");
                }
            }
        }
    }

    constexpr float THRUST_DISTANCE = 16.0f;
    constexpr float PIERCE_DISTANCE = 24.0f;
    constexpr float SLAM_THRUST = 8.0f;

    struct SwingVisual
    {
        float angle;
        float thrust;
    };

    float GetBaseAngle(Direction dir)
    {
        switch (dir)
        {
        case RIGHT:
            return 0.0f;
        case DOWN:
            return 90.0f;
        case LEFT:
            return 180.0f;
        case UP:
            return -90.0f;
        }
        return 0.0f;
    }

    SwingVisual CalcSwingVisual(const Attack &atk, Direction dir, float progress)
    {
        float startAngle = GetBaseAngle(dir) + atk.weapon->startAngleOffset;

        switch (atk.weapon->attackType)
        {
        case ATTACK_SLASH:
            return {Slash(atk.raycastAngle, progress), 0.0f};

        case ATTACK_THRUST:
            return {startAngle, progress * THRUST_DISTANCE};

        case ATTACK_PIERCE:
            return {startAngle, progress * -8.0f};

        case ATTACK_SLAM:
        {
            float slamProgress = progress * progress;
            return {
                startAngle + (slamProgress * atk.weapon->sweepAngle),
                slamProgress * SLAM_THRUST};
        }

        default:
            return {
                startAngle + (progress * atk.weapon->sweepAngle),
                0.0f};
        }
    }

    bool CheckRadialCollision(Vector2 origin, float attackAngle, float reach, float breadth, float attackerRadius, Rectangle targetHitbox)
    {
        Vector2 targetCenter = {
            targetHitbox.x + targetHitbox.width / 2.0f,
            targetHitbox.y + targetHitbox.height / 2.0f
        };
        float targetRadius = (targetHitbox.width + targetHitbox.height) / 4.0f;

        float dist = Vector2Distance(origin, targetCenter);
        float angleToTarget = atan2f(targetCenter.y - origin.y, targetCenter.x - origin.x) * (180.0f / PI);
        
        float diff = fmodf(angleToTarget - attackAngle + 540.0f, 360.0f) - 180.0f;
        float angleDiff = fabsf(diff);
        
        float forwardDist = dist * cosf(angleDiff * (PI / 180.0f));
        float lateralDist = dist * sinf(angleDiff * (PI / 180.0f));

        if (forwardDist >= -targetRadius && 
            forwardDist <= reach + attackerRadius + targetRadius && 
            lateralDist <= (breadth / 2.0f) + targetRadius)
        {
            return true;
        }
        return false;
    }

    Rectangle GetAttackAABB(Vector2 center, float angle, float reach, float breadth, float attackerRadius)
    {
        float rad = angle * (PI / 180.0f);
        Vector2 forward = { cosf(rad), sinf(rad) };
        Vector2 right = { -sinf(rad), cosf(rad) };
        
        Vector2 edgeCenter = { 
            center.x + forward.x * attackerRadius, 
            center.y + forward.y * attackerRadius 
        };
        
        Vector2 p1 = { edgeCenter.x + right.x * (breadth / 2.0f), edgeCenter.y + right.y * (breadth / 2.0f) };
        Vector2 p2 = { edgeCenter.x - right.x * (breadth / 2.0f), edgeCenter.y - right.y * (breadth / 2.0f) };
        Vector2 p3 = { p1.x + forward.x * reach, p1.y + forward.y * reach };
        Vector2 p4 = { p2.x + forward.x * reach, p2.y + forward.y * reach };
        
        float minX = std::min({p1.x, p2.x, p3.x, p4.x});
        float maxX = std::max({p1.x, p2.x, p3.x, p4.x});
        float minY = std::min({p1.y, p2.y, p3.y, p4.y});
        float maxY = std::max({p1.y, p2.y, p3.y, p4.y});
        
        return { minX, minY, maxX - minX, maxY - minY };
    }

    void ApplyHitToEntity(Player &player, Entity *target, Vector2 playerCenter)
    {
        Vector2 entityCenter = {target->Position.x + FRAME_SIZE / 2, target->Position.y + FRAME_SIZE / 2};
        Vector2 knockDir = Vector2Normalize(Vector2Subtract(entityCenter, playerCenter));

        float damage = player.attack.weapon->damage;
        Vector2 knockback = Vector2Scale(knockDir, player.attack.weapon->knockbackForce);
        target->TakeDamage(damage, knockback);

        Effects::AddDamage(entityCenter, damage);

        player.attack.damagedEntities.push_back(target);
        TraceLog(LOG_INFO, "COMBAT: Pemain mengenai musuh! Damage: %.1f", damage);
    }

    void CalcIdleWeaponPose(Direction dir, Direction lastHorizDir, float &outAngle, float &outOffsetRight)
    {
        outAngle = 0.0f;
        outOffsetRight = 0.0f;

        bool isRight = (lastHorizDir == RIGHT);

        switch (dir)
        {
        case RIGHT:
            outAngle = -9.0f;
            outOffsetRight = 0.8f;
            break;
        case LEFT:
            outAngle = 189.0f;
            break;
        case UP:
            outAngle = isRight ? -60.0f : -120.0f;
            outOffsetRight = isRight ? 0.8f : 0.0f;
            break;
        case DOWN:
            outAngle = isRight ? 60.0f : 120.0f;
            outOffsetRight = isRight ? 0.8f : 0.0f;
            break;
        }
    }

    void DrawSlashTrail(const Player &player, const std::string &spriteKey, float rayAngle)
    {
        float progress = player.attack.timer / player.attack.weapon->duration;
        std::string slashSpriteKey;

        if (progress >= 1.0f / 3.0f && progress < 2.0f / 3.0f)
        {
            slashSpriteKey = (spriteKey == "sword1") ? "slashLightGraySmall1" : "slashSilverBlueMedium1";
        }
        else if (progress >= 2.0f / 3.0f)
        {
            slashSpriteKey = (spriteKey == "sword1") ? "slashLightGraySmall2" : "slashSilverBlueMedium2";
        }

        if (slashSpriteKey.empty())
            return;

        const Frame &slashFrame = GetFrame(slashSpriteKey);
        float radRay = rayAngle * (PI / 180.0f);

        Vector2 slashPos = player.attack.center;
        float slashDist = player.attack.weapon->reach * 0.65f;
        slashPos.x += cosf(radRay) * slashDist;
        slashPos.y += sinf(radRay) * slashDist;

        if (progress >= 2.0f / 3.0f)
        {
            float shiftAngleRad = (rayAngle - 90.0f) * (PI / 180.0f);
            float H = (spriteKey == "sword1") ? 26.0f : 37.0f;
            float backDist = (spriteKey == "sword1") ? 16.0f : 19.5f;
            slashPos.x += cosf(shiftAngleRad) * H;
            slashPos.y += sinf(shiftAngleRad) * H;

            slashPos.x -= cosf(radRay) * backDist;
            slashPos.y -= sinf(radRay) * backDist;
        }

        Display slashDisplay;
        slashDisplay.position = slashPos;
        slashDisplay.size = 32;
        slashDisplay.offset = {0, 0};
        slashDisplay.origin = {
            (float)slashFrame.width * slashDisplay.size / 2.0f,
            (float)slashFrame.height * slashDisplay.size / 2.0f};
        slashDisplay.rotation = rayAngle + 90.0f;
        slashDisplay.tint = WHITE;

        DrawFrame(slashFrame, slashDisplay);
    }

    void PerformHitDetection(Player &player)
    {
        if (!player.attack.active || !player.attack.weapon)
            return;

        Vector2 playerCenter = player.GetCenter();
        float reach = player.attack.weapon->reach;
        float breadth = player.attack.weapon->breadth;
        float attackAngle = player.attack.raycastAngle;
        float attackerRadius = player.GetHitboxWidth() / 2.0f;

        for (auto *entity : Entities::GetRegistry())
        {
            Entity *playerAsEntity = &player;
            if (entity == playerAsEntity)
                continue;
            if (!entity->IsActive || entity->Health <= 0)
                continue;

            auto &dmg = player.attack.damagedEntities;
            if (std::find(dmg.begin(), dmg.end(), entity) != dmg.end())
                continue;

            if (CheckRadialCollision(playerCenter, attackAngle, reach, breadth, attackerRadius, entity->GetHitbox()))
            {
                ApplyHitToEntity(player, entity, playerCenter);
            }
        }

        Rectangle attackAABB = GetAttackAABB(playerCenter, attackAngle, reach, breadth, attackerRadius);
        HitPropsByAttack(attackAABB, PlayerInstance.GetHitbox(), &player);
    }

    void UpdateSwingAttack(Player &player, float dt)
    {
        if (!player.attack.active || !player.attack.weapon)
            return;

        player.attack.center = player.GetCenter();

        player.attack.timer += dt;
        if (player.attack.timer >= player.attack.weapon->duration)
        {
            player.attack.active = false;
            player.attack.timer = 0;
            player.Anim.isAttacking = false;
        }
        else
        {
            if (player.attack.weapon->attackType != ATTACK_PIERCE)
                PerformHitDetection(player);
        }
    }

    void DrawSwingAttack(Player &player)
    {
        InventoryItem item = Inventory::GetActiveHotbarItem(player);
        if (item.definitionId == -1)
            return;

        const ItemDefinition &def = itemDefs.GetById(item.definitionId);
        if (def.category != ITEM_WEAPON)
            return;

        Vector2 visualPos;
        float drawAngle;
        float thrust = 0.0f;
        float rayAngle;
        float offsetRight = 0.0f;

        if (player.attack.active)
        {
            float progress = player.attack.timer / player.attack.weapon->duration;
            SwingVisual visual = CalcSwingVisual(player.attack, player.Anim.direction, progress);

            visualPos = player.attack.center;
            drawAngle = visual.angle;
            thrust = visual.thrust;
            rayAngle = player.attack.raycastAngle;
        }
        else
        {
            visualPos = player.GetCenter();

            if (player.Anim.direction == RIGHT)
                player.LastHorizDir = RIGHT;
            else if (player.Anim.direction == LEFT)
                player.LastHorizDir = LEFT;

            CalcIdleWeaponPose(player.Anim.direction, player.LastHorizDir, rayAngle, offsetRight);
            drawAngle = rayAngle;
            thrust = 0.0f;
        }

        float rad = rayAngle * (PI / 180.0f);
        visualPos.x += cosf(rad) * thrust;
        visualPos.y += sinf(rad) * thrust;

        std::string spriteKey = def.spriteKey;
        if (spriteKey == "bow")
        {
            if (player.attack.active)
                spriteKey = "bow";
            else
                spriteKey = "bowDraw";
        }

        const Frame &frame = GetFrame(spriteKey);

        Display display;
        display.position = visualPos;
        display.size = 32;
        display.offset = {0, 1 + offsetRight};
        display.origin = {17.0f, (float)(frame.height * display.size)};
        display.rotation = drawAngle + 90.0f;
        display.tint = WHITE;

        DrawFrame(frame, display);

        if (player.attack.active && player.attack.weapon &&
            player.attack.weapon->attackType == ATTACK_SLASH &&
            (def.spriteKey == "sword1" || def.spriteKey == "sword2"))
        {
            DrawSlashTrail(player, def.spriteKey, player.attack.raycastAngle);
        }
    }
}

// Arrow Implementation
Arrow::Arrow(Vector2 pos, Vector2 dir, float speed, float damage, float reach, float rotation, Entity* owner, std::string spriteKey)
{
    StartPos = pos;
    Reach = reach;
    Position = pos;
    Velocity = Vector2Scale(Vector2Normalize(dir), speed);
    Damage = damage;
    Rotation = rotation;
    Owner = owner;
    SpriteKey = spriteKey;
    
    LifeTime = 0.0f;
    MaxLifeTime = 2.0f; // Arrows disappear after 2 seconds
    HasHit = false;
    IsActive = true;
    Health = 1.0f; // Arrow health
}

void Arrow::Update()
{
    if (!IsActive) return;

    float dt = Time::DELTA_TIME;
    LifeTime += dt;
    if (LifeTime >= MaxLifeTime)
    {
        IsActive = false;
        return;
    }

    Position = Vector2Add(Position, Vector2Scale(Velocity, dt));

    if (Vector2Distance(StartPos, Position) >= Reach)
    {
        IsActive = false;
        return;
    }

    // Check collision with map (walls)
    Rectangle hitbox = GetHitbox();
    if (CheckCollisionAgainstRects(hitbox, PlayerInstance.CollisionRects) || 
        CheckCollisionAgainstPolygons(hitbox, PlayerInstance.CollisionPolygons))
    {
        IsActive = false;
        return;
    }
    
    // Check collision with props
    if (CheckCollisionAgainstRects(hitbox, DynamicObstacles))
    {
        HitPropsByAttack(hitbox, PlayerInstance.GetHitbox(), &PlayerInstance);
        IsActive = false;
        return;
    }

    // Check collision with entities
    for (auto* entity : Entities::GetRegistry())
    {
        if (entity == this || entity == Owner || !entity->IsActive || entity->Health <= 0)
            continue;

        if (CheckCollisionRecs(hitbox, entity->GetHitbox()))
        {
            Vector2 knockback = Vector2Scale(Vector2Normalize(Velocity), 1.5f);
            entity->TakeDamage(Damage, knockback);
            
            // Show damage number at entity center
            Vector2 center = entity->GetCenter();
            Effects::AddDamage(center, Damage);
            
            IsActive = false;
            break;
        }
    }
}

void Arrow::Render()
{
    if (!IsActive) return;

    Display display;
    display.position = Position;
    display.size = 32;
    display.offset = {0, 0};
    display.origin = {16.0f, 16.0f};
    display.rotation = Rotation + 90.0f;
    display.tint = WHITE;

    DrawFrame(SpriteKey, display);
}

Rectangle Arrow::GetHitbox() const
{
    // A smaller hitbox for the arrow, centered
    return { Position.x - 4, Position.y - 4, 8, 8 };
}
