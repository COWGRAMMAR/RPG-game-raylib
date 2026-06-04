#include "effects.h"
#include "effectQueue.h"
#include "animation.h"
#include "player.h"
#include <vector>

namespace Effects {
    // --- Internal State ---
    static EffectQueue<DamagePopup> damageQueue;
    static EffectQueue<LogEntry> logQueue;

    // Constants for MessageLog
    static const float LOG_DURATION = 1.5f;
    static const float LOG_UP_SPEED = 30.0f;
    static const int LOG_LINE_HEIGHT = 12;

    void Update(float dt) {
        // 1. Update Damage Popups
        EffectNode<DamagePopup>* currentDmg = damageQueue.GetHead();
        while (currentDmg != nullptr) {
            DamagePopup& data = currentDmg->data;
            if (data.active) {
                data.timer += dt;
                if (data.timer >= data.duration) {
                    data.active = false;
                } else {
                    DamageFloat(data.position, data.velocity, 0.1f, 0.95f, dt);
                }
            }
            currentDmg = currentDmg->next;
        }

        // Cleanup inactive damage popups from head
        while (!damageQueue.IsEmpty() && !damageQueue.GetHead()->data.active) {
            damageQueue.Dequeue();
        }

        // 2. Update Message Log
        EffectNode<LogEntry>* currentLog = logQueue.GetHead();
        while (currentLog != nullptr) {
            LogEntry& entry = currentLog->data;
            entry.timer += dt;

            // Since it's a queue, we only cleanup from head if it's expired
            if (currentLog == logQueue.GetHead() && entry.timer >= LOG_DURATION) {
                logQueue.Dequeue();
                currentLog = logQueue.GetHead();
                continue;
            }

            entry.verticalOffset = TextFloat(entry.verticalOffset, LOG_UP_SPEED, dt);
            currentLog = currentLog->next;
        }
    }

    void Draw() {
        // 1. Draw Damage Popups
        EffectNode<DamagePopup>* currentDmg = damageQueue.GetHead();
        while (currentDmg != nullptr) {
            DamagePopup& data = currentDmg->data;
            if (data.active) {
                float alpha = FadeOut(data.timer, data.duration);
                Color color = Fade(YELLOW, alpha);
                
                std::string dmgStr = std::to_string((int)data.damage);
                int textW = MeasureText(dmgStr.c_str(), 10);
                DrawText(dmgStr.c_str(), (int)(data.position.x - textW / 2.0f), (int)data.position.y, 10, color);
            }
            currentDmg = currentDmg->next;
        }

        // 2. Draw Message Log
        Vector2 playerCenter = PlayerInstance.GetCenter();
        EffectNode<LogEntry>* currentLog = logQueue.GetHead();
        int index = 0;

        while (currentLog != nullptr) {
            LogEntry& entry = currentLog->data;
            
            float alpha = FadeOut(entry.timer, LOG_DURATION);
            int fontSize = 10;
            int textWidth = MeasureText(entry.text.c_str(), fontSize);
            
            // Calculate screen position relative to player head
            // Add (index * LOG_LINE_HEIGHT) to stack them vertically
            int x = (int)playerCenter.x - textWidth / 2;
            int y = (int)(playerCenter.y + entry.verticalOffset + (index * LOG_LINE_HEIGHT));

            // Draw shadow & text
            DrawText(entry.text.c_str(), x + 1, y + 1, fontSize, ColorAlpha(BLACK, alpha * 0.7f));
            DrawText(entry.text.c_str(), x, y, fontSize, ColorAlpha(WHITE, alpha));

            currentLog = currentLog->next;
            index++;
        }
    }

    void AddDamage(Vector2 pos, float damage) {
        DamagePopup p;
        p.position = pos;
        p.damage = damage;
        p.timer = 0;
        p.duration = 1.0f;
        p.velocity = {(float)GetRandomValue(-1200, 1200) / 10.0f, -120.0f};
        p.active = true;
        damageQueue.Enqueue(p);
    }

    void AddLog(const char* message) {
        logQueue.Enqueue({message, 0.0f, -20.0f});
        
        // Limit log size
        if (logQueue.Size() > 5) {
            logQueue.Dequeue();
        }
    }

    void Clear() {
        damageQueue.Clear();
        logQueue.Clear();
    }
}
