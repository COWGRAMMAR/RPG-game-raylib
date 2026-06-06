#include "gameOverScreen.h"
#include "player.h"
#include "item.h"
#include "inventory.h"
#include "../lib/raylib/include/raylib.h"
#include "button.h"

static Texture2D goTitle = {0};
static buttonImage reviveBtn;
static buttonImage settingsBtn;
static buttonImage goToMain;
static float goTitleY;
static bool goLoaded = false;

void InitGameOverScreen()
{
    if (goLoaded) return;

    Image img = LoadImage("assets/textures/gameOver/gameover.png");
    goTitle = LoadTextureFromImage(img);
    UnloadImage(img);

    img = LoadImage("assets/textures/gameOver/gameover-revive.png");
    float btnH = (float)img.height;
    UnloadImage(img);

    goTitleY = (GameScreenHeight - goTitle.height) / 2.0F - 80;
    float topY = goTitleY + goTitle.height + 10;
    float gap = 20.0f;

    // tombol Settings baru di antara Revive dan To Main dengan gap 20px
    Vector2 revivePos = {GameScreenWidth / 2.0F, topY + btnH / 2.0F};
    reviveBtn = buttonImage("assets/textures/gameOver/gameover-revive.png", revivePos, 1.0F, 0.6F);

    topY += btnH + gap;
    Vector2 settingsPos = {GameScreenWidth / 2.0F, topY + btnH / 2.0F};
    settingsBtn = buttonImage("assets/textures/gameOver/gameover-settings.png", settingsPos, 1.0F, 0.6F);

    topY += btnH + gap;
    Vector2 toMainPos = {GameScreenWidth / 2.0F, topY + btnH / 2.0F};
    goToMain = buttonImage("assets/textures/gameOver/gameover-to-main.png", toMainPos, 1.0F, 0.6F);

    goLoaded = true;
}

void UpdateGameOverScreen(GameState *state)
{
    Vector2 mousePos = GetVirtualMousePosition(state);
    bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (!goLoaded) InitGameOverScreen();

    if (reviveBtn.isClicked(mousePos, mouseClicked))
    {
        PlayerInstance.Anim.isDead = false;
        PlayerInstance.Anim.isAttacking = false;
        PlayAnimation(PlayerInstance.Anim, IDLE, PlayerInstance.Anim.direction);
        PlayerInstance.Health = PlayerInstance.MaxHealth;
        PlayerInstance.Mana = PlayerInstance.MaxMana;
        PlayerInstance.KnockbackVelocity = {0, 0};
        PlayerInstance.Position = state->startSpawnPos;
        PlayerInstance.hasDroppedItems = false;
        state->currentScreen = PLAY;
        return;
    }

    // klik Settings navigasi ke OPTIONS dengan previousScreen = GAME_OVER
    if (settingsBtn.isClicked(mousePos, mouseClicked))
    {
        state->previousScreen = GAME_OVER;
        state->currentScreen = OPTIONS;
        return;
    }

    if (goToMain.isClicked(mousePos, mouseClicked))
    {
        state->enteredLoading = false;
        state->loadingStage = 0;
        state->loadingProgress = 0.0F;
        state->loadingComplete = false;
        state->currentScreen = MAIN_MENU;
    }
}

void RenderGameOverScreen(GameState *state)
{
    if (!goLoaded) InitGameOverScreen();

    DrawRenderTexture(state);

    BeginTextureMode(state->Dungeon);

    DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, {0, 0, 0, 102});

    DrawTexture(goTitle, (GameScreenWidth - goTitle.width) / 2, (int)goTitleY, WHITE);

    Vector2 mousePos = GetVirtualMousePosition(state);
    reviveBtn.Draw(mousePos);
    settingsBtn.Draw(mousePos);
    goToMain.Draw(mousePos);

    EndTextureMode();
}
