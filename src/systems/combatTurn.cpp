#include "combatTurn.h"
#include "enemy.h"
#include "player.h"
#include "item.h"
#include "inventory.h"
#include "screen.h"
#include "audioManager.h"
#include "animation.h"
#include "map.h"
#include <raylib.h>
#include <raymath.h>
#include <cstdio>
#include "fonts.h"
#include <algorithm>

extern const int GameScreenWidth;
extern const int GameScreenHeight;
extern GameState *gState;

static struct
{
    bool active = false;
    TurnPhase phase = TurnPhase::INACTIVE;
    Enemy *boss = nullptr;
    Player *player = nullptr;
    std::string message;
    float timer = 0.0f;
    BossActionType lastBossAction;
    bool playerDefending = false;
    float combatTimer = 0.0f;
    bool keyProcessed = false;

    LootEntry loot[8];
    int lootCount = 0;

    float defeatCooldown = 0.0f;

    int selectedAction = -1;
    int selectedPotionId = -1;

    // Arena teleport
    Vector2 origPlayerPos;
    Vector2 origBossPos;
    Vector2 origCameraTarget;
    float origCameraZoom;

    // Smooth zoom transition
    float zoomTimer = 0.0f;
    float targetZoom;
    Vector2 targetCameraTarget;

    // Boss attack animation duration (for dynamic SHOW_RESULT timer)
    float bossAnimDuration = 0.0f;

} state;

static float OUTLINE_THICK = 3.0f;

void TurnCombat::Init(Enemy *boss, Player *player)
{
    state.active = true;
    state.phase = TurnPhase::PLAYER_CHOICE;
    state.boss = boss;
    state.player = player;
    state.message = "Pilih gerakan (1-3), lalu ENTER untuk konfirmasi!";
    state.timer = 0.0f;
    state.lastBossAction = BossActionType::CLAW;
    state.playerDefending = false;
    state.combatTimer = 0.0f;
    state.keyProcessed = false;
    state.selectedAction = -1;
    state.selectedPotionId = -1;
    // Save & teleport entities to arena
    state.origPlayerPos = player->Position;
    state.origBossPos = boss->Position;
    state.origCameraTarget = camera.target;
    state.origCameraZoom = camera.zoom;

    // Arena positions: below respective panels
    // At 2.5x zoom, screen width 1280 → world width ~512, screen height 720 → world height ~288
    // Panel center X: player=235, boss=1045. Screen center at 640.
    // World offset from camera target: (235-640)/2.5 = -162, (1045-640)/2.5 = +162
    // Panel bottom Y = 180, sprite below at ~230. (230-360)/2.5 = -52 above camera target
    float zoom = 2.5f;
    Vector2 center = player->GetCenter();
    float halfDist = 162.0f;
    float yOffset = -52.0f;

    Vector2 playerWorld = {center.x - halfDist, center.y + yOffset};
    Vector2 bossWorld = {center.x + halfDist, center.y + yOffset};

    player->Position.x = playerWorld.x - player->GetHitboxOffsetX() - player->GetHitboxWidth() / 2.0f;
    player->Position.y = playerWorld.y - player->GetHitboxOffsetY() - player->GetHitboxHeight() / 2.0f;
    boss->Position.x = bossWorld.x - boss->HitboxOffsetX - boss->HitboxWidth / 2.0f;
    boss->Position.y = bossWorld.y - boss->HitboxOffsetY - boss->HitboxHeight / 2.0f;

    // Start smooth zoom transition instead of instant jump
    state.targetCameraTarget = Vector2Scale(Vector2Add(playerWorld, bossWorld), 0.5f);
    state.targetZoom = zoom;
    state.zoomTimer = 0.5f;

    // Player faces right during combat
    PlayAnimation(player->Anim, IDLE, RIGHT);
}

static void TransitionTo(TurnPhase newPhase)
{
    state.phase = newPhase;
    state.timer = 0.0f;
    state.keyProcessed = false;
}

static float GetPlayerAttackDamage()
{
    return (float)GetRandomValue(25, 30);
}

static bool IsCriticalHit()
{
    return GetRandomValue(1, 100) <= 20;
}

static bool UsePotion(int defId)
{
    Player &p = *state.player;
    const ItemDefinition &def = itemDefs.GetById(defId);
    const PotionData &pot = std::get<PotionData>(def.data);
    float heal = (float)pot.healValue;

    auto scan = [&](int idx, bool isBag) -> bool
    {
        InventoryItem &item = isBag ? p.GetBagItem(idx) : p.GetHotbarItem(idx);
        if (item.definitionId != defId)
            return false;
        float oldHp = p.Health;
        p.Health = std::min(p.Health + heal, p.MaxHealth);
        item.amount--;
        if (item.amount <= 0)
            item = {-1, 0};
        char buf[64];
        snprintf(buf, sizeof(buf), "Menggunakan %s! +%.0f HP (%.0f -> %.0f)", def.name.c_str(), heal, oldHp, p.Health);
        state.message = buf;
        return true;
    };

    for (int i = 0; i < p.GetMaxHotbar(); i++)
        if (scan(i, false))
            return true;
    for (int i = 0; i < p.GetMaxBag(); i++)
        if (scan(i, true))
            return true;
    return false;
}

static int GetPotionIdForSlot(int slot)
{
    // slot 0 = small (id 2), 1 = medium (id 5), 2 = large (id 7)
    int potionIds[] = {2, 5, 7};
    if (slot >= 0 && slot < 3)
        return potionIds[slot];
    return -1;
}

static void GrantBossLoot()
{
    state.lootCount = 0;
    int droppedWeapons[3] = {-1, -1, -1};
    int weaponCount = 0;

    auto TryAdd = [](int defId, int amount)
    {
        ItemSpawn tmp;
        tmp.definitionId = defId;
        tmp.amount = amount;
        if (Inventory::AddToInventory(*state.player, tmp))
        {
            state.loot[state.lootCount++] = {defId, amount};
        }
    };

    auto IsWeaponDropped = [&](int defId) -> bool
    {
        for (int i = 0; i < weaponCount; i++)
            if (droppedWeapons[i] == defId)
                return true;
        return false;
    };

    int pool[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int poolSize = sizeof(pool) / sizeof(pool[0]);

    int count = GetRandomValue(2, 3);
    for (int i = 0; i < count; i++)
    {
        int attempts = 0;
        int defId;
        const ItemDefinition *def;
        do
        {
            int idx = GetRandomValue(0, poolSize - 1);
            defId = pool[idx];
            def = &itemDefs.GetById(defId);
            attempts++;
        } while (def->category == ITEM_WEAPON && IsWeaponDropped(defId) && attempts < 20);

        if (def->category == ITEM_WEAPON && IsWeaponDropped(defId))
            continue;

        int amount;
        if (def->category == ITEM_WEAPON)
        {
            amount = 1;
            droppedWeapons[weaponCount++] = defId;
        }
        else
            switch (def->rarity)
            {
            case RARITY_COMMON:
                amount = GetRandomValue(5, 8);
                break;
            case RARITY_UNCOMMON:
                amount = GetRandomValue(3, 5);
                break;
            case RARITY_RARE:
                amount = GetRandomValue(1, 3);
                break;
            default:
                amount = 1;
                break;
            }

        TryAdd(defId, amount);
    }
}

static void ExecuteBossTurn()
{
    float roll = (float)GetRandomValue(0, 99) / 100.0f;
    float damage = 0;
    const char *actionName = "";
    State animState = ATTACK;

    if (roll < 0.15f)
    {
        state.lastBossAction = BossActionType::DASH;
        damage = state.player->Health * 0.5f;
        actionName = "Trample";
        animState = ABILITY2;
    }
    else if (roll < 0.50f)
    {
        state.lastBossAction = BossActionType::BITE;
        damage = (float)GetRandomValue(8, 20);
        actionName = "Bantingan";
        animState = ABILITY1;
    }
    else
    {
        state.lastBossAction = BossActionType::CLAW;
        damage = (float)GetRandomValue(3, 15);
        actionName = "Pukulan";
        animState = ATTACK;
    }

    // Play boss attack animation (boss faces left toward player in arena)
    if (state.boss)
    {
        PlayAnimation(state.boss->Anim, animState, LEFT);
        // Calculate total animation duration from config
        if (state.boss->Anim.currentConfig && !state.boss->Anim.currentConfig->sprites.empty())
            state.bossAnimDuration = (float)state.boss->Anim.currentConfig->sprites.size() * state.boss->Anim.currentConfig->speed;
        else
            state.bossAnimDuration = 1.0f;
    }

    if (state.playerDefending)
    {
        damage *= 0.5f;
        state.playerDefending = false;
        char buf[96];
        snprintf(buf, sizeof(buf), "Boss menggunakan %s! (Damage berkurang) %.0f damage diterima", actionName, damage);
        state.message = buf;
    }
    else
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "Boss menggunakan %s! %.0f damage diterima!", actionName, damage);
        state.message = buf;
    }

    state.player->TakeDamage(damage, {0, 0});

    if (state.player->Health <= 0)
    {
        state.player->Health = 0;
    }
    state.combatTimer = 0.0f;
}

void TurnCombat::Update()
{
    if (!state.active)
        return;

    state.combatTimer += Time::DELTA_TIME;

    // Smooth zoom transition at start
    if (state.zoomTimer > 0.0f)
    {
        state.zoomTimer -= Time::DELTA_TIME;
        float t = 1.0f - state.zoomTimer / 0.5f;
        camera.zoom = Lerp(state.origCameraZoom, state.targetZoom, t);
        camera.target.x = Lerp(state.origCameraTarget.x, state.targetCameraTarget.x, t);
        camera.target.y = Lerp(state.origCameraTarget.y, state.targetCameraTarget.y, t);
        if (state.zoomTimer <= 0.0f)
        {
            camera.zoom = state.targetZoom;
            camera.target = state.targetCameraTarget;
        }
        return;
    }

    switch (state.phase)
    {
    case TurnPhase::PLAYER_CHOICE:
    {
        if (IsKeyPressed(KEY_ONE))
            state.selectedAction = (state.selectedAction == 0) ? -1 : 0;
        else if (IsKeyPressed(KEY_TWO))
            state.selectedAction = (state.selectedAction == 1) ? -1 : 1;
        else if (IsKeyPressed(KEY_THREE))
            state.selectedAction = (state.selectedAction == 2) ? -1 : 2;

        if (state.selectedAction >= 0)
        {
            const char *names[] = {"Attack", "Items", "Defense"};
            state.message = TextFormat("Dipilih: %s. Tekan ENTER untuk konfirmasi.", names[state.selectedAction]);
        }
        else
        {
            state.message = "Pilih gerakan (1-3), lalu ENTER untuk konfirmasi!";
        }

        if (IsKeyPressed(KEY_ENTER) && state.selectedAction >= 0)
        {
            state.combatTimer = 0.0f;
            if (state.selectedAction == 0)
                state.phase = TurnPhase::PLAYER_ATTACK;
            else if (state.selectedAction == 1)
                state.phase = TurnPhase::PLAYER_ITEM;
            else if (state.selectedAction == 2)
            {
                state.playerDefending = true;
                state.message = "Bertahan untuk serangan berikutnya!";
                state.phase = TurnPhase::PLAYER_DEFEND;
            }
        }
        break;
    }

    case TurnPhase::PLAYER_ATTACK:
    {
        if (!state.keyProcessed)
        {
            float dmg = GetPlayerAttackDamage();
            bool critical = IsCriticalHit();
            if (critical)
                dmg *= 2.0f;
            float oldHp = state.boss->Health;
            state.boss->Health = std::max(0.0f, state.boss->Health - dmg);
            char buf[96];
            if (critical)
                snprintf(buf, sizeof(buf), "CRITICAL! %.0f damage! (%.0f -> %.0f)", dmg, oldHp, state.boss->Health);
            else
                snprintf(buf, sizeof(buf), "Kamu menyerang memberikan %.0f damage! (%.0f -> %.0f)", dmg, oldHp, state.boss->Health);
            state.message = buf;
            state.keyProcessed = true;
            state.timer = 1.0f;
            state.boss->HitFlashTimer = 0.3f;
            AudioManager::PlaySFX("slash-short");

            // Trigger sword swing arc in front of player
            Player &p = *state.player;
            PlayAnimation(p.Anim, IDLE, RIGHT);
            InventoryItem activeItem = Inventory::GetActiveHotbarItem(p);
            if (activeItem.definitionId != -1)
            {
                const ItemDefinition &def = itemDefs.GetById(activeItem.definitionId);
                if (def.category == ITEM_WEAPON)
                {
                    p.attack.active = true;
                    p.attack.timer = 0.0f;
                    p.attack.center = p.GetCenter();
                    p.attack.damagedEntities.clear();
                    p.Anim.isAttacking = true;
                    p.attack.raycastAngle = 0.0f;
                    Inventory::SetupAttackStats(p, RIGHT);
                    p.attack.center.y -= 6.0f;
                }
            }
        }
        state.timer -= Time::DELTA_TIME;
        state.player->attack.timer += Time::DELTA_TIME;
        if (state.timer <= 0.0f)
        {
            // Reset player attack state before transitioning
            state.player->attack.active = false;
            state.player->attack.timer = 0.0f;
            state.player->Anim.isAttacking = false;
            if (state.boss->Health <= 0)
                TransitionTo(TurnPhase::VICTORY);
            else
                TransitionTo(TurnPhase::SHOW_RESULT);
        }
        break;
    }

    case TurnPhase::PLAYER_ITEM:
    {
        if (state.keyProcessed)
        {
            // After successful potion use: countdown, then transition
            state.timer -= Time::DELTA_TIME;
            if (state.timer <= 0.0f)
            {
                if (state.boss->Health <= 0)
                    TransitionTo(TurnPhase::VICTORY);
                else
                    TransitionTo(TurnPhase::SHOW_RESULT);
            }
        }
        else if (state.timer > 0.0f)
        {
            // 1s delay after "no potion" message
            state.timer -= Time::DELTA_TIME;
            state.message = "Tidak ada potion! Kembali ke menu...";
            if (state.timer <= 0.0f)
            {
                state.selectedPotionId = -1;
                TransitionTo(TurnPhase::PLAYER_CHOICE);
            }
        }
        else
        {
            // Normal input: select potion type, then ENTER to confirm
            if (IsKeyPressed(KEY_ONE))
                state.selectedPotionId = GetPotionIdForSlot(0);
            else if (IsKeyPressed(KEY_TWO))
                state.selectedPotionId = GetPotionIdForSlot(1);
            else if (IsKeyPressed(KEY_THREE))
                state.selectedPotionId = GetPotionIdForSlot(2);

            if (state.selectedPotionId >= 0)
            {
                const ItemDefinition &def = itemDefs.GetById(state.selectedPotionId);
                state.message = TextFormat("Dipilih: %s. Tekan ENTER untuk memakai.", def.name.c_str());
            }
            else
            {
                state.message = "Pilih potion (1-3), lalu ENTER untuk konfirmasi!";
            }

            if (IsKeyPressed(KEY_ENTER) && state.selectedPotionId >= 0)
            {
                if (UsePotion(state.selectedPotionId))
                {
                    state.keyProcessed = true;
                    state.timer = 1.0f;
                }
                else
                {
                    const ItemDefinition &def = itemDefs.GetById(state.selectedPotionId);
                    state.message = TextFormat("Tidak ada %s!", def.name.c_str());
                    state.selectedPotionId = -1;
                    state.timer = 1.0f;
                }
            }
        }
        break;
    }

    case TurnPhase::PLAYER_DEFEND:
    {
        if (!state.keyProcessed)
        {
            state.keyProcessed = true;
            state.timer = 1.0f;
        }
        state.timer -= Time::DELTA_TIME;
        if (state.timer <= 0.0f)
        {
            TransitionTo(TurnPhase::SHOW_RESULT);
        }
        break;
    }

    case TurnPhase::SHOW_RESULT:
    {
        if (!state.keyProcessed)
        {
            ExecuteBossTurn();
            state.keyProcessed = true;
            // Use dynamic timer matching the boss attack animation duration
            state.timer = state.bossAnimDuration;
        }
        state.timer -= Time::DELTA_TIME;
        if (state.timer <= 0.0f)
        {
            if (state.player->Health <= 0)
                TransitionTo(TurnPhase::DEFEAT);
            else
            {
                // Reset boss back to idle animation
                if (state.boss)
                    PlayAnimation(state.boss->Anim, IDLE, LEFT);
                TransitionTo(TurnPhase::PLAYER_CHOICE);
            }
        }
        break;
    }

    case TurnPhase::BOSS_TURN:
    {
        ExecuteBossTurn();
        break;
    }

    case TurnPhase::VICTORY:
    {
        if (state.lootCount == 0)
        {
            GrantBossLoot();
            state.message = "Boss dikalahkan! Loot didapat:";
            AudioManager::PlayTrack("Win");
        }
        if (state.combatTimer >= 1.0f && (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
        {
            state.boss->Health = 0;
            state.boss->IsActive = false;
            Shutdown();
        }
        break;
    }

    case TurnPhase::DEFEAT:
    {
        if (!state.keyProcessed)
        {
            PlayAnimation(state.player->Anim, DEAD, RIGHT);
            state.player->Anim.isDead = true;
            DropAllItems(*state.player);
            state.player->hasDroppedItems = true;
            state.keyProcessed = true;
        }
        if (state.combatTimer >= 2.0f || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            state.defeatCooldown = 6.0f;
            Shutdown();
        }
        break;
    }

    default:
        break;
    }
}

static void DrawHealthBar(float x, float y, float w, float h, float ratio, Color color)
{
    DrawRectangleRounded((Rectangle){x + 2, y + 2, w, h}, 0.3f, 8, ColorAlpha(BLACK, 0.4f));
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.3f, 8, DARKGRAY);
    if (ratio > 0)
    {
        DrawRectangleRounded((Rectangle){x, y, w * ratio, h}, 0.3f, 8, color);
        DrawRectangleRounded((Rectangle){x, y, w * ratio, h * 0.3f}, 0.3f, 8, ColorAlpha(WHITE, 0.15f));
    }
    DrawRectangleRoundedLinesEx((Rectangle){x, y, w, h}, 0.3f, 8, 1.5f, ColorAlpha(WHITE, 0.2f));
}

static void DrawTextCentered(const char *text, int y, int fontSize, Color color)
{
    int textW = MeasureText(text, fontSize);
    DrawDefaultText(text, (GameScreenWidth - textW) / 2, y, fontSize, color);
}

static void DrawActionButton(const char *key, const char *label, int x, int y, int w, int h, bool selected, bool highlight)
{
    Color bg = highlight ? ColorAlpha(GOLD, 0.3f) : ColorAlpha(DARKGRAY, 0.7f);
    Color border = highlight ? GOLD : ColorAlpha(WHITE, 0.3f);
    if (selected)
        border = RED;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.3f, 8, bg);
    DrawRectangleRoundedLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.3f, 8, selected ? OUTLINE_THICK * 1.5f : OUTLINE_THICK, border);

    char fullLabel[64];
    snprintf(fullLabel, sizeof(fullLabel), "[%s] %s", key, label);
    int fontSize = 20;
    int textW = MeasureText(fullLabel, fontSize);
    DrawDefaultText(fullLabel, x + (w - textW) / 2, y + (h - fontSize) / 2, fontSize, highlight ? GOLD : WHITE);
}

void TurnCombat::Draw()
{
    if (!state.active)
        return;

    int topBorder = 20;
    DrawRectangleRoundedLinesEx((Rectangle){10, (float)topBorder, (float)GameScreenWidth - 20, (float)GameScreenHeight - (float)topBorder * 2}, 0.1f, 8, OUTLINE_THICK, ColorAlpha(GOLD, 0.5f));

    DrawTextCentered("TURN-BASED COMBAT", 30, 28, GOLD);

    int panelY = 70;
    int panelH = 110;
    int panelW = 350;

    // Player panel (left)
    int playerX = 60;
    DrawRectangleRounded((Rectangle){(float)playerX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(BLACK, 0.5f));
    DrawRectangleRoundedLinesEx((Rectangle){(float)playerX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, OUTLINE_THICK, ColorAlpha(BLUE, 0.5f));
    DrawDefaultText("PLAYER", playerX + 15, panelY + 10, 18, BLUE);

    float hp = state.player->Health;
    float maxHp = state.player->MaxHealth;
    float hpRatio = maxHp > 0 ? hp / maxHp : 0;
    float barW = panelW - 40;
    DrawHealthBar((float)playerX + 15, (float)panelY + 40, barW, 20, hpRatio, RED);

    char hpText[32];
    snprintf(hpText, sizeof(hpText), "HP: %.0f / %.0f", hp, maxHp);
    DrawDefaultText(hpText, playerX + 15, panelY + 70, 16, WHITE);

    float mana = state.player->Mana;
    float maxMana = state.player->MaxMana;
    char manaText[32];
    snprintf(manaText, sizeof(manaText), "MP: %.0f / %.0f", mana, maxMana);
    DrawDefaultText(manaText, playerX + 15, panelY + 90, 16, GOLD);

    // Boss panel (right)
    int bossX = GameScreenWidth - 60 - panelW;
    DrawRectangleRounded((Rectangle){(float)bossX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, ColorAlpha(BLACK, 0.5f));
    DrawRectangleRoundedLinesEx((Rectangle){(float)bossX, (float)panelY, (float)panelW, (float)panelH}, 0.2f, 8, OUTLINE_THICK, ColorAlpha(RED, 0.5f));

    const char *bossName = state.boss->Def ? state.boss->Def->name.c_str() : "BOSS";
    char bossLabel[64];
    snprintf(bossLabel, sizeof(bossLabel), "BOSS: %s", bossName);
    DrawDefaultText(bossLabel, bossX + 15, panelY + 10, 18, RED);

    float bossHp = state.boss->Health;
    float bossMaxHp = state.boss->MaxHealth;
    float bossRatio = bossMaxHp > 0 ? bossHp / bossMaxHp : 0;
    DrawHealthBar((float)bossX + 15, (float)panelY + 40, barW, 20, bossRatio, RED);

    char bossHpText[32];
    snprintf(bossHpText, sizeof(bossHpText), "HP: %.0f / %.0f", bossHp, bossMaxHp);
    DrawDefaultText(bossHpText, bossX + 15, panelY + 70, 16, WHITE);

    if (state.playerDefending)
    {
        DrawDefaultText(">>> BERTAHAN! <<<", bossX + 15, panelY + 90, 14, GREEN);
    }

    // Boss sprite area — below boss panel
    int spriteY = panelY + panelH + 10;
    int bossSpriteX = bossX + panelW / 2 - FRAME_SIZE / 2;
    Rectangle bossSpriteArea = {(float)bossSpriteX, (float)spriteY, (float)FRAME_SIZE, (float)FRAME_SIZE};

    // Boss flash handled by Enemy::Render() via HitFlashTimer

    // Slash effect removed per user request

    // Message area
    int msgY = panelY + panelH + 40;
    bool isCritical = state.message.rfind("CRITICAL", 0) == 0;
    DrawTextCentered(state.message.c_str(), msgY, 22, isCritical ? RED : YELLOW);

    // Action buttons (bottom)
    if (state.phase == TurnPhase::PLAYER_CHOICE)
    {
        int btnY = GameScreenHeight - 80;
        int btnW = 180;
        int btnH = 50;
        int totalW = btnW * 3 + 20 * 2;
        int startX = (GameScreenWidth - totalW) / 2;

        DrawActionButton("1", "Attack", startX, btnY, btnW, btnH, state.selectedAction == 0, false);
        DrawActionButton("2", "Items", startX + btnW + 20, btnY, btnW, btnH, state.selectedAction == 1, false);
        DrawActionButton("3", "Defense", startX + (btnW + 20) * 2, btnY, btnW, btnH, state.selectedAction == 2, false);
    }
    else if (state.phase == TurnPhase::PLAYER_ITEM)
    {
        int btnY = GameScreenHeight - 80;
        int btnW = 180;
        int btnH = 50;
        int totalW = btnW * 3 + 20 * 2;
        int startX = (GameScreenWidth - totalW) / 2;
        const char *potionNames[] = {"Small HP", "Medium HP", "Large HP"};
        int potionDefs[] = {2, 5, 7};
        int selPotion = state.selectedPotionId;
        DrawActionButton("1", potionNames[0], startX, btnY, btnW, btnH, selPotion == potionDefs[0], false);
        DrawActionButton("2", potionNames[1], startX + btnW + 20, btnY, btnW, btnH, selPotion == potionDefs[1], false);
        DrawActionButton("3", potionNames[2], startX + (btnW + 20) * 2, btnY, btnW, btnH, selPotion == potionDefs[2], false);
    }
    else if (state.phase == TurnPhase::VICTORY)
    {
        // Darken screen
        DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.5f));

        // Big MENANG text with yellow outline
        const char *menangText = "MENANG";
        int fontSize = 80;
        int textW = MeasureText(menangText, fontSize);
        int textX = (GameScreenWidth - textW) / 2;
        int textY = GameScreenHeight / 2 - fontSize / 2 - 40;

        DrawDefaultText(menangText, textX - 3, textY, fontSize, YELLOW);
        DrawDefaultText(menangText, textX + 3, textY, fontSize, YELLOW);
        DrawDefaultText(menangText, textX, textY - 3, fontSize, YELLOW);
        DrawDefaultText(menangText, textX, textY + 3, fontSize, YELLOW);
        DrawDefaultText(menangText, textX, textY, fontSize, WHITE);

        // Loot list below the title (icon + text centered as group)
        int lootY = textY + fontSize + 20;
        for (int i = 0; i < state.lootCount; i++)
        {
            int itemY = lootY + i * 38;
            const ItemDefinition &def = itemDefs.GetById(state.loot[i].definitionId);
            char itemText[64];
            snprintf(itemText, sizeof(itemText), "%s x%d", def.name.c_str(), state.loot[i].amount);
            int iconSize = 28;
            int gap = 8;
            int textW = MeasureText(itemText, 20);
            int groupW = iconSize + gap + textW;
            int groupX = (GameScreenWidth - groupW) / 2;
            Rectangle iconDest = {(float)groupX, (float)itemY - 3, (float)iconSize, (float)iconSize};
            Display iconDisplay = {{iconDest.x, iconDest.y}, (int)iconDest.width, {0, 0}, {0, 0}, 0.0f, WHITE};
            DrawFrame(def.spriteKey, iconDisplay);
            DrawDefaultText(itemText, groupX + iconSize + gap, itemY + 4, 20, LIGHTGRAY);
        }

        DrawTextCentered("Tekan ENTER atau klik untuk melanjutkan.", GameScreenHeight - 60, 20, GREEN);
    }
    else if (state.phase == TurnPhase::DEFEAT)
    {
        // Darken screen
        DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.5f));
    }

    // Phase indicator
    const char *phaseText = "";
    switch (state.phase)
    {
    case TurnPhase::PLAYER_CHOICE:
        phaseText = "Giliran anda";
        break;
    case TurnPhase::PLAYER_ATTACK:
        phaseText = "Menyerang...";
        break;
    case TurnPhase::PLAYER_ITEM:
        phaseText = "Menggunakan Item...";
        break;
    case TurnPhase::PLAYER_DEFEND:
        phaseText = "Bertahan...";
        break;
    case TurnPhase::SHOW_RESULT:
        phaseText = "Boss sedang menyerang!";
        break;
    case TurnPhase::BOSS_TURN:
        phaseText = "Giliran Boss";
        break;
    case TurnPhase::VICTORY:
        phaseText = "MENANG!";
        break;
    case TurnPhase::DEFEAT:
        phaseText = "";
        break;
    default:
        break;
    }
    DrawTextCentered(phaseText, GameScreenHeight - 120, 18, LIGHTGRAY);
}

bool TurnCombat::IsActive()
{
    return state.active;
}

void TurnCombat::Shutdown()
{
    TurnPhase currentPhase = state.phase;
    state.active = false;
    state.phase = TurnPhase::INACTIVE;
    if (state.boss)
    {
        state.boss->isTurnBasedMode = false;
        state.boss->Position = state.origBossPos;
    }
    if (state.player)
    {
        if (currentPhase == TurnPhase::DEFEAT)
            state.player->Position = gState->startSpawnPos;
        else
            state.player->Position = state.origPlayerPos;
    }
    state.boss = nullptr;
    state.player = nullptr;
    state.message.clear();
    state.playerDefending = false;

    // Restore camera
    camera.target = state.origCameraTarget;
    camera.zoom = state.origCameraZoom;

    // Resume normal screen-based music
    AudioManager::ResetToScreenTrack();
}

TurnPhase TurnCombat::GetPhase()
{
    return state.phase;
}

const std::string &TurnCombat::GetMessage()
{
    return state.message;
}

BossActionType TurnCombat::GetLastBossAction()
{
    return state.lastBossAction;
}

bool TurnCombat::WasPlayerDefending()
{
    return state.playerDefending;
}

int TurnCombat::GetLootCount()
{
    return state.lootCount;
}

const LootEntry *TurnCombat::GetLoot(int index)
{
    if (index < 0 || index >= state.lootCount)
        return nullptr;
    return &state.loot[index];
}

float TurnCombat::GetDefeatCooldown()
{
    return state.defeatCooldown;
}

void TurnCombat::UpdateCooldown()
{
    if (state.defeatCooldown > 0.0f)
        state.defeatCooldown -= Time::DELTA_TIME;
}
