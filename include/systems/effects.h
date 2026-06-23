#pragma once
#include "raylib.h"
#include <string>

// --- Data Structures ---

/**
 * @brief Data for a damage number popup
 */
struct DamagePopup
{
    Vector2 position;
    Vector2 velocity;
    float damage;
    float timer;
    float duration;
    bool active;
};

/**
 * @brief Data for a message log entry
 */
struct LogEntry
{
    std::string text;
    float timer;
    float verticalOffset;
};

// --- Effects System ---

/**
 * @brief Unified system for managing queue-based visual effects.
 * Similar to the Entities system, it provides a central hub for all transient visual feedback.
 */
namespace Effects
{
    /**
     * @brief Update all active effect queues (timers, physics, cleanup).
     */
    void Update(float dt);

    /**
     * @brief Render all active effects to the screen.
     */
    void Draw();

    /**
     * @brief Add a new damage popup at the specified position.
     */
    void AddDamage(Vector2 pos, float damage);

    /**
     * @brief Add a new message to the player's log.
     */
    void AddLog(const char *message);

    /**
     * @brief Add a projectile collision effect (1x1 slam-style animation) at a specific position.
     */
    void AddCollision(Vector2 pos);

    /**
     * @brief Clear all active effects.
     */
    void Clear();
}
