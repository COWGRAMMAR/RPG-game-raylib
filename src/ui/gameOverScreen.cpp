#include "gameOverScreen.h"
#include "player.h"
#include "item.h"
#include "inventory.h"
#include "../lib/raylib/include/raylib.h"
#include "button.h"

static Texture2D goTitle = {0};
static buttonImage reviveBtn;
static buttonImage goToMain;
static float goTitleY, goToMainY;
static bool goLoaded = false;

void InitGameOverScreen()
{
    if (goLoaded) return;

    Image img = LoadImage("assets/textures/gameOver/gameover.png");
    goTitle = LoadTextureFromImage(img);
    UnloadImage(img);

    img = LoadImage("assets/textures/gameOver/gameover-revive.png");
    float reviveH = (float)img.height;
    UnloadImage(img);

    goTitleY = (GameScreenHeight - goTitle.height) / 2.0F - 80;
    float goReviveTopY = goTitleY + goTitle.height + 10;
    goToMainY = goReviveTopY + reviveH + 85;

    Vector2 revivePos = {GameScreenWidth / 2.0F, goReviveTopY + reviveH / 2.0F};
    reviveBtn = buttonImage("assets/textures/gameOver/gameover-revive.png", revivePos, 1.0F, 0.6F);

    Vector2 toMainPos = {GameScreenWidth / 2.0F, goToMainY};
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
    goToMain.Draw(mousePos);

    EndTextureMode();
}
