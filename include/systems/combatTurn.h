#pragma once
#include "../lib/raylib/include/raylib.h"
#include <string>

class Enemy;
class Player;

enum class TurnPhase {
    INACTIVE,
    PLAYER_CHOICE,
    PLAYER_ATTACK,
    PLAYER_ITEM,
    PLAYER_DEFEND,
    BOSS_TURN,
    SHOW_RESULT,
    VICTORY,
    DEFEAT
};

enum class BossActionType {
    CLAW,
    BITE,
    DASH
};

struct LootEntry {
    int definitionId;
    int amount;
};

namespace TurnCombat {
    void Init(Enemy* boss, Player* player);
    void Update();
    void Draw();
    bool IsActive();
    void Shutdown();

    TurnPhase GetPhase();
    const std::string& GetMessage();
    BossActionType GetLastBossAction();
    bool WasPlayerDefending();
    int GetLootCount();
    const LootEntry* GetLoot(int index);
    float GetDefeatCooldown();
    void UpdateCooldown();
}
