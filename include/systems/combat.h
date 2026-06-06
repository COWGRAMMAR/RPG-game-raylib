#pragma once
#include "raylib.h"
#include "effects.h"
#include "item.h"
#include <vector>
#include <string>
#include "entity.h"

class Arrow : public Entity
{
public:
    Vector2 Velocity;
    Vector2 StartPos;
    float Reach;
    float Rotation;
    float LifeTime;
    float MaxLifeTime;
    float Damage;
    Entity* Owner;
    bool HasHit;
    std::string SpriteKey;

    Arrow(Vector2 pos, Vector2 dir, float speed, float damage, float reach, float rotation, Entity* owner, std::string spriteKey = "arrow");
    
    void Update() override;
    void Render() override;
    Rectangle GetHitbox() const override;
};

class Player;

class Entity;

namespace Combat
{
    struct Attack
    {
        bool active = false;
        float timer = 0.0f;
        float duration = 0.0f;
        float raycastAngle = 0.0f;
        Vector2 center = {0, 0};
        std::vector<Entity *> damagedEntities;
        bool pressHeld = false;
        const WeaponData* weapon = nullptr;
    };

    void Update(Player &player);
    void HandleDead(Player &player);
    void HandleStamina(Player &player);
    void HandleAttack(Player &player);
    /** @brief Update animasi swing attack */
    void UpdateSwingAttack(Player &player, float dt);
    /** @brief Render visual swing attack */
    void DrawSwingAttack(Player &player);

}
