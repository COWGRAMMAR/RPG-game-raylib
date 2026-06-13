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
#include "raymath.h"
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

        HandleStamina(player);

        if (InputInstance.IsInventoryOpen())
            return;

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

        if (player.Mana < player.MaxMana)
        {
            player.Mana += player.ManaRegenRate * Time::DELTA_TIME;
            if (player.Mana > player.MaxMana)
                player.Mana = player.MaxMana;
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
                        Effects::AddLog("Tidak Ada Senjata!");
                        player.attack.pressHeld = false;
                        return;
                    }

                    player.Mana -= manaCost;
                    player.ManaRegenTimer = player.ManaRegenDelay;

                    player.attack.active = true;
                    player.attack.timer = 0;
                    player.attack.damagedEntities.clear();
                    player.attack.center = playerCenter;
                    player.attack.startCenter = playerCenter;
                    player.attack.raycastAngle = angle;

                    Inventory::SetupAttackStats(player, attackFaceDir);

                    if (player.attack.weapon && player.attack.weapon->attackType == ATTACK_THRUST)
                    {
                        player.IsDashing = true;
                        player.DashDuration = player.DashDurationMax;
                        player.DashSpeed = player.DashMaxSpeed;
                        AudioManager::PlaySFX("dash");
                    }

                    player.Anim.isAttacking = true;
                    TraceLog(LOG_INFO, "PLAYER: Serangan diarahkan ke (%.2f, %.2f)", attackDir.x, attackDir.y);

                    const ItemDefinition &def = itemDefs.GetById(activeItem.definitionId);
                    if (player.attack.weapon)
                    {
                        if (player.attack.weapon->attackType == ATTACK_THRUST || def.spriteKey == "spear")
                        {
                            AudioManager::PlaySFX("thrust");
                        }
                        else if (def.spriteKey == "sword1")
                        {
                            AudioManager::PlaySFX("slash-short");
                        }
                        else if (def.spriteKey == "sword2" || def.spriteKey == "axe" || player.attack.weapon->attackType == ATTACK_SLASH)
                        {
                            AudioManager::PlaySFX("slash-mid");
                        }
                        else if (def.spriteKey == "bow")
                        {
                            AudioManager::PlaySFX("arrow");
                        }
                        else if (def.spriteKey == "ak47")
                        {
                            AudioManager::PlaySFX("rifle");
                        }
                        else if (player.attack.weapon->attackType == ATTACK_SLAM || def.spriteKey == "hammer")
                        {
                            AudioManager::PlaySFX("slam");
                        }
                    }

                    if (player.attack.weapon->attackType == ATTACK_PIERCE)
                    {
                        float arrowSpeed = (player.attack.weapon->duration > 0.0f) ? (player.attack.weapon->reach / player.attack.weapon->duration) : 300.0f;
                        std::string projSprite = (def.spriteKey == "ak47") ? "bullet" : "arrow";
                        float arrowDamage = player.attack.weapon->damage * player.BuffDamageMultiplier;
                        Arrow *arrow = new Arrow(playerCenter, attackDir, arrowSpeed, arrowDamage, player.attack.weapon->reach, angle, &player, player.attack.weapon->knockbackForce, projSprite);
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

    SwingVisual CalcSwingVisual(const Attack &atk, Direction dir, Direction lastHorizDir, float progress, const std::string &spriteKey)
    {
        bool isRight = (lastHorizDir == RIGHT);
        float sign = isRight ? 1.0f : -1.0f;
        float startAngle = GetBaseAngle(dir) + (atk.weapon->startAngleOffset * sign);

        switch (atk.weapon->attackType)
        {
        case ATTACK_SLASH:
            if (spriteKey == "sword2" || spriteKey == "axe")
                return {SwingMidMid(atk.raycastAngle, progress, isRight), 0.0f};
            return {SwingShortMid(atk.raycastAngle, progress, isRight), 0.0f};

        case ATTACK_THRUST:
        {
            float thrustProgress = 0.0f;
            if (progress < 0.25f)
            {
                // Menusuk dengan cepat dalam 25% durasi pertama (Ease-out)
                float t = progress / 0.25f;
                thrustProgress = t * (2.0f - t);
            }
            else
            {
                // Menarik kembali tombak secara bertahap dalam 75% durasi sisanya
                float t = (progress - 0.25f) / 0.75f;
                thrustProgress = 1.0f - t;
            }
            // Jarak tusukan yang lebih dinamis berdasarkan jangkauan (reach) senjata
            float maxThrust = std::max(24.0f, atk.weapon->reach * 0.4f);
            return {atk.raycastAngle, thrustProgress * maxThrust};
        }

        case ATTACK_PIERCE:
            return {atk.raycastAngle, progress * -8.0f};

        case ATTACK_SLAM:
        {
            float slamProgress = progress * progress;
            if (spriteKey == "hammer")
                return {SwingShortSlow(atk.raycastAngle, progress, isRight), slamProgress * SLAM_THRUST};
            return {
                startAngle + (slamProgress * atk.weapon->sweepAngle * sign),
                slamProgress * SLAM_THRUST};
        }

        default:
            return {
                startAngle + (progress * atk.weapon->sweepAngle * sign),
                0.0f};
        }
    }

    bool CheckRadialCollision(Vector2 origin, float attackAngle, float reach, float breadth, float attackerRadius, Rectangle targetHitbox)
    {
        Vector2 targetCenter = {
            targetHitbox.x + targetHitbox.width / 2.0f,
            targetHitbox.y + targetHitbox.height / 2.0f};
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
        Vector2 forward = {cosf(rad), sinf(rad)};
        Vector2 right = {-sinf(rad), cosf(rad)};

        Vector2 edgeCenter = {
            center.x + forward.x * attackerRadius,
            center.y + forward.y * attackerRadius};

        Vector2 p1 = {edgeCenter.x + right.x * (breadth / 2.0f), edgeCenter.y + right.y * (breadth / 2.0f)};
        Vector2 p2 = {edgeCenter.x - right.x * (breadth / 2.0f), edgeCenter.y - right.y * (breadth / 2.0f)};
        Vector2 p3 = {p1.x + forward.x * reach, p1.y + forward.y * reach};
        Vector2 p4 = {p2.x + forward.x * reach, p2.y + forward.y * reach};

        float minX = std::min({p1.x, p2.x, p3.x, p4.x});
        float maxX = std::max({p1.x, p2.x, p3.x, p4.x});
        float minY = std::min({p1.y, p2.y, p3.y, p4.y});
        float maxY = std::max({p1.y, p2.y, p3.y, p4.y});

        return {minX, minY, maxX - minX, maxY - minY};
    }

    void ApplyHitToEntity(Player &player, Entity *target, Vector2 playerCenter)
    {
        Vector2 entityCenter = {target->Position.x + FRAME_SIZE / 2, target->Position.y + FRAME_SIZE / 2};
        Vector2 knockDir = Vector2Normalize(Vector2Subtract(entityCenter, playerCenter));

        float damage = player.attack.weapon->damage * player.BuffDamageMultiplier;
        Vector2 knockback = Vector2Scale(knockDir, player.attack.weapon->knockbackForce);
        target->TakeDamage(damage, knockback);

        Effects::AddDamage(entityCenter, damage);

        player.attack.damagedEntities.push_back(target);
        TraceLog(LOG_INFO, "COMBAT: Pemain mengenai musuh! Damage: %.1f", damage);
    }

    void CalcIdleWeaponPose(Direction dir, Direction lastHorizDir, float &outAngle, float &outOffsetRight, const std::string &spriteKey)
    {
        outAngle = 0.0f;
        outOffsetRight = 0.8f;

        bool isRight = (lastHorizDir == RIGHT);
        bool exactHorizontal = (spriteKey == "bow" || spriteKey == "bowDraw" || spriteKey == "ak47");

        switch (dir)
        {
        case RIGHT:
            outAngle = exactHorizontal ? 0.0f : -9.0f;
            break;
        case LEFT:
            outAngle = exactHorizontal ? 180.0f : 189.0f;
            break;
        case UP:
            outAngle = isRight ? -60.0f : -120.0f;
            break;
        case DOWN:
            outAngle = isRight ? 60.0f : 120.0f;
            break;
        }
    }

    void DrawThrustEffect(const Player &player, const std::string &spriteKey, float rayAngle)
    {
        float progress = player.attack.timer / player.attack.weapon->duration;
        std::string effectSpriteKey;

        if (progress >= 0.2f && progress < 0.5f)
            effectSpriteKey = "thrust0101";
        else if (progress >= 0.5f && progress < 0.8f)
            effectSpriteKey = "thrust0102";
        else if (progress >= 0.8f)
            effectSpriteKey = "thrust0103";

        if (effectSpriteKey.empty())
            return;

        const Frame &frame = GetFrame(effectSpriteKey);
        float radRay = rayAngle * (PI / 180.0f);

        Vector2 pos = player.attack.center;
        float dist = player.attack.weapon->reach * 0.8f;
        pos.x += cosf(radRay) * dist;
        pos.y += sinf(radRay) * dist;

        Display display;
        display.position = pos;
        display.size = 32;
        display.offset = {0, 0};
        display.origin = {
            (float)frame.width * display.size / 2.0f,
            (float)frame.height * display.size / 2.0f};
        display.rotation = rayAngle;
        display.tint = WHITE;
        display.flip = false;

        DrawFrame(frame, display);
    }

    void DrawSlashTrail(const Player &player, const std::string &spriteKey, float rayAngle)
    {
        float progress = player.attack.timer / player.attack.weapon->duration;
        std::string slashSpriteKey;

        bool isShort = (spriteKey == "sword1");

        if (progress >= 2.0f / 5.0f && progress < 3.0f / 5.0f)
        {
            slashSpriteKey = isShort ? "slashShort0101" : "slashMedium0101";
        }
        else if (progress >= 3.0f / 5.0f && progress < 4.0f / 5.0f)
        {
            slashSpriteKey = isShort ? "slashShort0102" : "slashMedium0102";
        }
        else if (progress >= 4.0f / 5.0f)
        {
            slashSpriteKey = isShort ? "slashShort0101" : "slashMedium0101";
        }

        if (slashSpriteKey.empty())
            return;

        const Frame &slashFrame = GetFrame(slashSpriteKey);
        float radRay = rayAngle * (PI / 180.0f);

        Vector2 slashPos = player.attack.center;
        float slashDist = player.attack.weapon->reach * 0.5f;
        slashPos.x += cosf(radRay) * slashDist;
        slashPos.y += sinf(radRay) * slashDist;

        bool isRight = (player.LastHorizDir == RIGHT);
        float sign = isRight ? 1.0f : -1.0f;

        if (progress >= 3.0f / 5.0f && progress < 4.0f / 5.0f)
        {
            float shiftAngleRad = (rayAngle + (90.0f * sign)) * (PI / 180.0f);
            float H = isShort ? 26.0f : 37.0f;
            float backDist = isShort ? 16.0f : 19.5f;
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

        float renderAngle = rayAngle;
        if (progress >= 4.0f / 5.0f)
        {
            renderAngle = rayAngle + 90.0f * sign;
        }

        slashDisplay.rotation = renderAngle;
        slashDisplay.tint = WHITE;
        slashDisplay.flip = !isRight;

        DrawFrame(slashFrame, slashDisplay);
    }

    void PerformHitDetection(Player &player)
    {
        if (!player.attack.active || !player.attack.weapon)
            return;

        Vector2 attackCenter = (player.attack.weapon->attackType == ATTACK_SLAM) ? player.attack.startCenter : player.attack.center;
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

            bool hit = false;
            if (player.attack.weapon->attackType == ATTACK_SLAM)
            {
                float size = reach * 2.0f;
                Rectangle slamAABB = {
                    attackCenter.x - reach,
                    attackCenter.y - reach,
                    size,
                    size
                };
                if (CheckCollisionRecs(slamAABB, entity->GetHitbox()))
                {
                    hit = true;
                }
            }
            else
            {
                if (CheckRadialCollision(attackCenter, attackAngle, reach, breadth, attackerRadius, entity->GetHitbox()))
                {
                    hit = true;
                }
            }

            if (hit)
            {
                ApplyHitToEntity(player, entity, attackCenter);
            }
        }

        Rectangle attackAABB;
        if (player.attack.weapon->attackType == ATTACK_SLAM)
        {
            float size = reach * 2.0f;
            attackAABB = {
                attackCenter.x - reach,
                attackCenter.y - reach,
                size,
                size
            };
        }
        else
        {
            attackAABB = GetAttackAABB(attackCenter, attackAngle, reach, breadth, attackerRadius);
        }
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

        std::string spriteKey = def.spriteKey;
        if (spriteKey == "bow")
        {
            if (player.attack.active)
                spriteKey = "bow";
            else
                spriteKey = "bowDraw";
        }

        if (player.attack.active)
        {
            float progress = player.attack.timer / player.attack.weapon->duration;
            SwingVisual visual = CalcSwingVisual(player.attack, player.Anim.direction, player.LastHorizDir, progress, spriteKey);

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

            CalcIdleWeaponPose(player.Anim.direction, player.LastHorizDir, rayAngle, offsetRight, spriteKey);
            drawAngle = rayAngle;
            thrust = 0.0f;
        }

        float rad = rayAngle * (PI / 180.0f);
        visualPos.x += cosf(rad) * thrust;
        visualPos.y += sinf(rad) * thrust;

        const Frame &frame = GetFrame(spriteKey);

        bool isRight = (player.LastHorizDir == RIGHT);

        Display display;
        display.position = visualPos;
        display.size = 32;
        display.offset = {0, 1 + offsetRight};
        display.tint = WHITE;
        display.flip = !isRight;

        float renderAngle = drawAngle;
        Vector2 origin = {0.0f, 17.0f};

        if (!isRight)
        {
            renderAngle = drawAngle - 180.0f;
            origin = {(float)(frame.width * display.size), 17.0f};
        }

        display.origin = origin;
        display.rotation = renderAngle;

        DrawFrame(frame, display);

        if (player.attack.active && player.attack.weapon)
        {
            // if (player.attack.weapon->attackType == ATTACK_SLASH &&
            //     (def.spriteKey == "sword1" || def.spriteKey == "sword2" || def.spriteKey == "axe"))
            // {
            //     DrawSlashTrail(player, def.spriteKey, player.attack.raycastAngle);
            // }
            // else if (player.attack.weapon->attackType == ATTACK_THRUST)
            // {
            //     DrawThrustEffect(player, def.spriteKey, player.attack.raycastAngle);
            // }

            // if (player.attack.weapon->attackType == ATTACK_SLAM)
            // {
            //     float progress = player.attack.timer / player.attack.weapon->duration;
            //     float tX = std::floor(player.attack.startCenter.x / 32.0f);
            //     float tY = std::floor(player.attack.startCenter.y / 32.0f);

            //     for (int x = -2; x <= 2; ++x)
            //     {
            //         for (int y = -2; y <= 2; ++y)
            //         {
            //             Vector2 tilePos = {
            //                 (tX + x) * 32.0f,
            //                 (tY + y) * 32.0f
            //             };
            //             Collision(tilePos, progress);
            //         }
            //     }
            // }
        }
    }

    void DrawSwingGroundEffect(Player &player)
    {
        if (player.attack.active && player.attack.weapon)
        {
            if (player.attack.weapon->attackType == ATTACK_SLAM)
            {
                float progress = player.attack.timer / player.attack.weapon->duration;
                float reach = player.attack.weapon->reach;
                float scaleTiles = (reach * 2.0f) / 32.0f;

                Collision(player.attack.startCenter, progress, scaleTiles);
            }
        }
    }
}

Arrow::Arrow(Vector2 pos, Vector2 dir, float speed, float damage, float reach, float rotation, Entity *owner, float knockbackForce, std::string spriteKey)
{
    StartPos = pos;
    Reach = reach;
    Position = pos;
    Velocity = Vector2Scale(Vector2Normalize(dir), speed);
    Damage = damage;
    KnockbackForce = knockbackForce;
    Rotation = rotation;
    Owner = owner;
    SpriteKey = spriteKey;
    LifeTime = 0.0f;
    MaxLifeTime = 2.0f;
    HasHit = false;
    IsActive = true;
    Health = 1.0f;
}

void Arrow::Update()
{
    if (!IsActive)
        return;

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
        Effects::AddCollision(Position);
        IsActive = false;
        return;
    }

    Rectangle hitbox = GetHitbox();
    if (CheckCollisionAgainstRects(hitbox, PlayerInstance.CollisionRects) ||
        CheckCollisionAgainstPolygons(hitbox, PlayerInstance.CollisionPolygons))
    {
        Effects::AddCollision(Position);
        IsActive = false;
        return;
    }

    if (CheckCollisionAgainstRects(hitbox, DynamicObstacles))
    {
        HitPropsByAttack(hitbox, PlayerInstance.GetHitbox(), &PlayerInstance);
        Effects::AddCollision(Position);
        IsActive = false;
        return;
    }

    for (auto *entity : Entities::GetRegistry())
    {
        if (entity == this || entity == Owner || !entity->IsActive || entity->Health <= 0)
            continue;

        if (CheckCollisionRecs(hitbox, entity->GetHitbox()))
        {
            Vector2 knockback = Vector2Scale(Vector2Normalize(Velocity), KnockbackForce);
            entity->TakeDamage(Damage, knockback);

            Vector2 center = entity->GetCenter();
            Effects::AddDamage(center, Damage);
            
            Effects::AddCollision(Position);
            IsActive = false;
            break;
        }
    }
}

void Arrow::Render()
{
    if (!IsActive)
        return;

    Display display;
    display.position = Position;
    display.size = 32;
    display.offset = {0, 0};
    display.origin = {16.0f, 16.0f};
    display.rotation = Rotation;
    display.tint = WHITE;

    DrawFrame(SpriteKey, display);
}

Rectangle Arrow::GetHitbox() const
{
    return {Position.x - 4, Position.y - 4, 8, 8};
}
