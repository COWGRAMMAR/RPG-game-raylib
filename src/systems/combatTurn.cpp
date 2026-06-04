#include "combatTurn.h"
#include "enemy.h"
#include "player.h"
#include "item.h"
#include "inventory.h"
#include "screen.h"
#include "animation.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/raylib/include/raymath.h"
#include <cstdio>
#include <algorithm>

extern const int GameScreenWidth;
extern const int GameScreenHeight;
extern GameState *gState;

static struct {
    bool active = false;
    TurnPhase phase = TurnPhase::INACTIVE;
    Enemy* boss = nullptr;
    Player* player = nullptr;
    std::string message;
    float timer = 0.0f;
    BossActionType lastBossAction;
    bool playerDefending = false;
    float combatTimer = 0.0f;
    bool keyProcessed = false;

    LootEntry loot[8];
    int lootCount = 0;

    float defeatCooldown = 0.0f;
} state;

void TurnCombat::Init(Enemy* boss, Player* player) {
    state.active = true;
    state.phase = TurnPhase::PLAYER_CHOICE;
    state.boss = boss;
    state.player = player;
    state.message = "Pilih gerakan yang ingin dilakukan!";
    state.timer = 0.0f;
    state.lastBossAction = BossActionType::CLAW;
    state.playerDefending = false;
    state.combatTimer = 0.0f;
    state.keyProcessed = false;
}

static void TransitionTo(TurnPhase newPhase) {
    state.phase = newPhase;
    state.timer = 0.0f;
    state.keyProcessed = false;
}

static float GetPlayerAttackDamage() {
    ItemSlot slot = InputInstance.GetActiveSlot();
    int slotIdx = (int)slot - 1;
    if (slotIdx >= 0 && slotIdx < state.player->GetMaxHotbar()) {
        InventoryItem& item = state.player->GetHotbarItem(slotIdx);
        if (item.definitionId != -1) {
            const ItemDefinition& def = itemDefs.GetById(item.definitionId);
            if (def.category == ITEM_WEAPON) {
                const WeaponData& w = std::get<WeaponData>(def.data);
                return w.damage;
            }
        }
    }
    return 15.0f;
}

static bool UseHealthPotion() {
    Player& p = *state.player;
    for (int i = 0; i < p.GetMaxHotbar(); i++) {
        if (p.GetHotbarItem(i).definitionId == 2) {
            const ItemDefinition& def = itemDefs.GetById(2);
            const PotionData& pot = std::get<PotionData>(def.data);
            float heal = (float)pot.healValue;
            float oldHp = p.Health;
            p.Health = std::min(p.Health + heal, p.MaxHealth);
            p.GetHotbarItem(i).amount--;
            if (p.GetHotbarItem(i).amount <= 0)
                p.GetHotbarItem(i) = {-1, 0};
            char buf[64];
            snprintf(buf, sizeof(buf), "Menggunakan Health Potion! +%.0f HP (%.0f -> %.0f)", heal, oldHp, p.Health);
            state.message = buf;
            return true;
        }
    }
    for (int i = 0; i < p.GetMaxBag(); i++) {
        if (p.GetBagItem(i).definitionId == 2) {
            const ItemDefinition& def = itemDefs.GetById(2);
            const PotionData& pot = std::get<PotionData>(def.data);
            float heal = (float)pot.healValue;
            float oldHp = p.Health;
            p.Health = std::min(p.Health + heal, p.MaxHealth);
            p.GetBagItem(i).amount--;
            if (p.GetBagItem(i).amount <= 0)
                p.GetBagItem(i) = {-1, 0};
            char buf[64];
            snprintf(buf, sizeof(buf), "Menggunakan Health Potion! +%.0f HP (%.0f -> %.0f)", heal, oldHp, p.Health);
            state.message = buf;
            return true;
        }
    }
    state.message = "Tidak ada potions! Giliran anda terbuang.";
    return false;
}

static void GrantBossLoot() {
    state.lootCount = 0;

    auto TryAdd = [](int defId, int amount) {
        ItemSpawn tmp;
        tmp.definitionId = defId;
        tmp.amount = amount;
        if (Inventory::AddToInventory(*state.player, tmp)) {
            state.loot[state.lootCount++] = {defId, amount};
        }
    };

    TryAdd(2, 3); // 3 Health Potions
    TryAdd(3, 2); // 2 Mana Bread
}

static void ExecuteBossTurn() {
    float roll = (float)GetRandomValue(0, 99) / 100.0f;
    float damage = 0;
    const char* actionName = "";

    if (roll < 0.15f) {
        state.lastBossAction = BossActionType::DASH;
        damage = state.player->Health * 0.5f;
        actionName = "Dash";
    } else if (roll < 0.50f) {
        state.lastBossAction = BossActionType::BITE;
        damage = (float)GetRandomValue(8, 20);
        actionName = "Gigitan";
    } else {
        state.lastBossAction = BossActionType::CLAW;
        damage = (float)GetRandomValue(3, 15);
        actionName = "Cakaran";
    }

    if (state.playerDefending) {
        damage *= 0.5f;
        state.playerDefending = false;
        char buf[96];
        snprintf(buf, sizeof(buf), "Boss menggunakan %s! (Damage berkurang) %.0f damage diterima", actionName, damage);
        state.message = buf;
    } else {
        char buf[96];
        snprintf(buf, sizeof(buf), "Boss menggunakan %s! %.0f damage diterima!", actionName, damage);
        state.message = buf;
    }

    state.player->TakeDamage(damage, {0, 0});

    if (state.player->Health <= 0) {
        state.player->Health = 0;
        TransitionTo(TurnPhase::DEFEAT);
    } else {
        TransitionTo(TurnPhase::PLAYER_CHOICE);
    }
    state.combatTimer = 0.0f;
}

void TurnCombat::Update() {
    if (!state.active) return;

    state.combatTimer += GetFrameTime();

    switch (state.phase) {
    case TurnPhase::PLAYER_CHOICE: {
        if (IsKeyPressed(KEY_ONE)) {
            state.phase = TurnPhase::PLAYER_ATTACK;
            state.combatTimer = 0.0f;
        } else if (IsKeyPressed(KEY_TWO)) {
            state.phase = TurnPhase::PLAYER_ITEM;
            state.combatTimer = 0.0f;
        } else if (IsKeyPressed(KEY_THREE)) {
            state.playerDefending = true;
            state.message = "Bertahan untuk serangan berikutnya!";
            state.phase = TurnPhase::PLAYER_DEFEND;
            state.combatTimer = 0.0f;
        }
        break;
    }

    case TurnPhase::PLAYER_ATTACK: {
        float dmg = GetPlayerAttackDamage();
        float oldHp = state.boss->Health;
        state.boss->Health = std::max(0.0f, state.boss->Health - dmg);
        char buf[64];
        snprintf(buf, sizeof(buf), "Kamu menyerang memberikan %.0f damage! (%.0f -> %.0f)", dmg, oldHp, state.boss->Health);
        state.message = buf;

        if (state.boss->Health <= 0) {
            TransitionTo(TurnPhase::VICTORY);
        } else {
            TransitionTo(TurnPhase::SHOW_RESULT);
        }
        break;
    }

    case TurnPhase::PLAYER_ITEM: {
        UseHealthPotion();
        if (state.boss->Health <= 0) {
            TransitionTo(TurnPhase::VICTORY);
        } else {
            TransitionTo(TurnPhase::SHOW_RESULT);
        }
        break;
    }

    case TurnPhase::PLAYER_DEFEND: {
        TransitionTo(TurnPhase::SHOW_RESULT);
        break;
    }

    case TurnPhase::SHOW_RESULT: {
        if (state.combatTimer >= 1.0f) {
            ExecuteBossTurn();
        }
        break;
    }

    case TurnPhase::BOSS_TURN: {
        ExecuteBossTurn();
        break;
    }

    case TurnPhase::VICTORY: {
        if (state.lootCount == 0) {
            GrantBossLoot();
            state.message = "Boss dikalahkan! Loot didapat:";
        }
        if (state.combatTimer >= 1.0f && (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
            state.boss->Health = 0;
            state.boss->IsActive = false;
            Shutdown();
        }
        break;
    }

    case TurnPhase::DEFEAT: {
        if (state.combatTimer >= 2.0f || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.player->Health = 0;
            state.defeatCooldown = 6.0f;
            Shutdown();
        }
        break;
    }

    default:
        break;
    }
}

static void DrawHealthBar(float x, float y, float w, float h, float ratio, Color color) {
    DrawRectangleRounded((Rectangle){x + 2, y + 2, w, h}, 0.3f, 8, ColorAlpha(BLACK, 0.4f));
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.3f, 8, DARKGRAY);
    if (ratio > 0) {
        DrawRectangleRounded((Rectangle){x, y, w * ratio, h}, 0.3f, 8, color);
        DrawRectangleRounded((Rectangle){x, y, w * ratio, h * 0.3f}, 0.3f, 8, ColorAlpha(WHITE, 0.15f));
    }
    DrawRectangleRoundedLines((Rectangle){x, y, w, h}, 0.3f, 8, ColorAlpha(WHITE, 0.2f));
}

static void DrawTextCentered(const char* text, int y, int fontSize, Color color) {
    int textW = MeasureText(text, fontSize);
    DrawText(text, (GameScreenWidth - textW) / 2, y, fontSize, color);
}

static void DrawActionButton(const char* key, const char* label, int x, int y, int w, int h, bool highlight) {
    Color bg = highlight ? ColorAlpha(GOLD, 0.3f) : ColorAlpha(DARKGRAY, 0.7f);
    Color border = highlight ? GOLD : ColorAlpha(WHITE, 0.3f);
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.3f, 8, bg);
    DrawRectangleRoundedLines((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.3f, 8, border);

    char fullLabel[64];
    snprintf(fullLabel, sizeof(fullLabel), "[%s] %s", key, label);
    int fontSize = 20;
    int textW = MeasureText(fullLabel, fontSize);
    DrawText(fullLabel, x + (w - textW) / 2, y + (h - fontSize) / 2, fontSize, highlight ? GOLD : WHITE);
}

void TurnCombat::Draw() {
    if (!state.active) return;

    DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.75f));

    int topBorder = 20;
    DrawRectangleRoundedLines((Rectangle){10, (float)topBorder, (float)GameScreenWidth - 20, (float)GameScreenHeight - (float)topBorder * 2}, 0.1f, 8, ColorAlpha(GOLD, 0.5f));

    DrawTextCentered("TURN-BASED COMBAT", 30, 28, GOLD);

    int panelY = 70;
    int panelH = 110;
    int panelW = 350;

    // Player panel (left)
    int playerX = 60;
    DrawRectangleRounded((Rectangle){(float)playerX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(BLACK, 0.5f));
    DrawRectangleRoundedLines((Rectangle){(float)playerX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(BLUE, 0.5f));
    DrawText("PLAYER", playerX + 15, panelY + 10, 18, BLUE);

    float hp = state.player->Health;
    float maxHp = state.player->MaxHealth;
    float hpRatio = maxHp > 0 ? hp / maxHp : 0;
    float barW = panelW - 40;
    DrawHealthBar((float)playerX + 15, (float)panelY + 40, barW, 20, hpRatio, RED);

    char hpText[32];
    snprintf(hpText, sizeof(hpText), "HP: %.0f / %.0f", hp, maxHp);
    DrawText(hpText, playerX + 15, panelY + 70, 16, WHITE);

    float mana = state.player->Mana;
    float maxMana = state.player->MaxMana;
    char manaText[32];
    snprintf(manaText, sizeof(manaText), "MP: %.0f / %.0f", mana, maxMana);
    DrawText(manaText, playerX + 15, panelY + 90, 16, GOLD);

    // Boss panel (right)
    int bossX = GameScreenWidth - 60 - panelW;
    DrawRectangleRounded((Rectangle){(float)bossX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(BLACK, 0.5f));
    DrawRectangleRoundedLines((Rectangle){(float)bossX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(RED, 0.5f));

    const char* bossName = state.boss->Def ? state.boss->Def->name.c_str() : "BOSS";
    char bossLabel[64];
    snprintf(bossLabel, sizeof(bossLabel), "BOSS: %s", bossName);
    DrawText(bossLabel, bossX + 15, panelY + 10, 18, RED);

    float bossHp = state.boss->Health;
    float bossMaxHp = state.boss->MaxHealth;
    float bossRatio = bossMaxHp > 0 ? bossHp / bossMaxHp : 0;
    DrawHealthBar((float)bossX + 15, (float)panelY + 40, barW, 20, bossRatio, RED);

    char bossHpText[32];
    snprintf(bossHpText, sizeof(bossHpText), "HP: %.0f / %.0f", bossHp, bossMaxHp);
    DrawText(bossHpText, bossX + 15, panelY + 70, 16, WHITE);

    if (state.playerDefending) {
        DrawText(">>> BERTAHAN! <<<", bossX + 15, panelY + 90, 14, GREEN);
    }

    // Message area
    int msgY = panelY + panelH + 40;
    DrawTextCentered(state.message.c_str(), msgY, 22, YELLOW);

    // Action buttons (bottom)
    if (state.phase == TurnPhase::PLAYER_CHOICE) {
        int btnY = GameScreenHeight - 80;
        int btnW = 180;
        int btnH = 50;
        int totalW = btnW * 3 + 20 * 2;
        int startX = (GameScreenWidth - totalW) / 2;

        DrawActionButton("1", "Attack", startX, btnY, btnW, btnH, false);
        DrawActionButton("2", "Items", startX + btnW + 20, btnY, btnW, btnH, false);
        DrawActionButton("3", "Defense", startX + (btnW + 20) * 2, btnY, btnW, btnH, false);
    } else if (state.phase == TurnPhase::VICTORY) {
        // Loot panel (right side)
        int lootX = GameScreenWidth - 60 - 320;
        int lootY = panelY + panelH + 20;
        int lootW = 320;
        int lootH = 60 + state.lootCount * 45;
        DrawRectangleRounded((Rectangle){(float)lootX, (float)lootY, (float)lootW, (float)lootH}, 0.2f, 8, ColorAlpha(BLACK, 0.6f));
        DrawRectangleRoundedLines((Rectangle){(float)lootX, (float)lootY, (float)lootW, (float)lootH}, 0.2f, 8, ColorAlpha(GREEN, 0.5f));
        DrawText("LOOT", lootX + 15, lootY + 10, 20, GREEN);

        for (int i = 0; i < state.lootCount; i++) {
            int itemY = lootY + 40 + i * 45;
            const ItemDefinition &def = itemDefs.GetById(state.loot[i].definitionId);
            Rectangle iconDest = {(float)lootX + 15, (float)itemY, 32, 32};
            Display iconDisplay = {{iconDest.x, iconDest.y}, (int)iconDest.width, {0,0}, {0,0}, 0.0f, WHITE};
            DrawFrame(def.spriteKey, iconDisplay);
            char itemText[64];
            snprintf(itemText, sizeof(itemText), "%s x%d", def.name.c_str(), state.loot[i].amount);
            DrawText(itemText, lootX + 55, itemY + 6, 18, WHITE);
        }

        DrawTextCentered("Tekan ENTER atau klik untuk melanjutkan.", GameScreenHeight - 60, 20, GREEN);
    } else if (state.phase == TurnPhase::DEFEAT) {
        DrawTextCentered("KALAH! Tekan ENTER atau klik untuk melanjutkan.", GameScreenHeight - 60, 24, RED);
    }

    // Phase indicator
    const char* phaseText = "";
    switch (state.phase) {
    case TurnPhase::PLAYER_CHOICE: phaseText = "Giliran anda"; break;
    case TurnPhase::PLAYER_ATTACK: phaseText = "Menyerang..."; break;
    case TurnPhase::PLAYER_ITEM: phaseText = "Menggunakan Item..."; break;
    case TurnPhase::PLAYER_DEFEND: phaseText = "Bertahan..."; break;
    case TurnPhase::SHOW_RESULT: phaseText = "Boss sedang menyerang!"; break;
    case TurnPhase::BOSS_TURN: phaseText = "Giliran Boss"; break;
    case TurnPhase::VICTORY: phaseText = "MENANG!"; break;
    case TurnPhase::DEFEAT: phaseText = "KALAH!"; break;
    default: break;
    }
    DrawTextCentered(phaseText, GameScreenHeight - 120, 18, LIGHTGRAY);
}

bool TurnCombat::IsActive() {
    return state.active;
}

void TurnCombat::Shutdown() {
    state.active = false;
    state.phase = TurnPhase::INACTIVE;
    if (state.boss)
        state.boss->isTurnBasedMode = false;
    state.boss = nullptr;
    state.player = nullptr;
    state.message.clear();
    state.playerDefending = false;
}

TurnPhase TurnCombat::GetPhase() {
    return state.phase;
}

const std::string& TurnCombat::GetMessage() {
    return state.message;
}

BossActionType TurnCombat::GetLastBossAction() {
    return state.lastBossAction;
}

bool TurnCombat::WasPlayerDefending() {
    return state.playerDefending;
}

int TurnCombat::GetLootCount() {
    return state.lootCount;
}

const LootEntry* TurnCombat::GetLoot(int index) {
    if (index < 0 || index >= state.lootCount) return nullptr;
    return &state.loot[index];
}

float TurnCombat::GetDefeatCooldown() {
    return state.defeatCooldown;
}

void TurnCombat::UpdateCooldown() {
    if (state.defeatCooldown > 0.0f)
        state.defeatCooldown -= GetFrameTime();
}
