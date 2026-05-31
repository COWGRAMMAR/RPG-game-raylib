#pragma once
#include "../lib/raylib/include/raylib.h"
#include <string>
#include <vector>
#include <unordered_map>

/*
====================
Frames Management
====================
*/

constexpr int FRAME_SIZE = 32;
constexpr int FRAME_GAP = 4;
constexpr int MAX_TEXTURES = 7;

extern Texture2D textures[MAX_TEXTURES];

enum TextureSlot
{
    TILESET_MAP_1,
    TILESET_MAP_2,
    TILESET_PROPS,
    TILESET_ITEMS,
    SPRITESHEET_KNIGHT,
    SPRITESHEET_ENEMIES,
    SPRITESHEET_EFFECTS
};

struct Frame
{
    TextureSlot texture;
    int positionX;
    int positionY;
    int width = 1;
    int height = 1;
};

struct Display
{
    Vector2 position;
    int size = FRAME_SIZE;
    Vector2 offset = {0, 0};
    Vector2 origin = {0, 0};
    float rotation = 0.0f;
    Color tint = WHITE;
    bool flip = false;
};

void LoadFrameTexture(TextureSlot slot, const char *path);
void InitTextures();
void CloseTextures();
const Frame &GetFrame(const std::string &id);
void DrawFrame(Frame frame, Display display);
void DrawFrame(const std::string &id, Display display);

/*
====================
Sprites Animation
====================
*/

enum State
{
    IDLE,
    WALK,
    ATTACK,
    DEAD
};

enum Direction
{
    LEFT,
    RIGHT,
    DOWN,
    UP
};

struct AnimationConfig
{
    std::vector<std::string> sprites;
    float speed;
    bool loop;
};

struct AnimationSet
{
    AnimationConfig configs[4][4];
};

struct Animation
{
    Vector2 position;
    State state;
    Direction direction;
    float timer;
    int currentSpriteIndex;
    bool isAttacking;
    bool isDead;
    const AnimationConfig *currentConfig;
    const AnimationSet *animSet;
};

void LoadAnimationsFromJSON();
void PlayAnimation(Animation &anim, State state, Direction direction);
void UpdateAnimation(Animation &anim, float dt);
void DrawAnimation(const Animation &anim, Color tint = WHITE);
void Explosion(Vector2 centerPosition, float radius, float progress);

extern std::unordered_map<std::string, AnimationSet> loadedAnimationSets;

/*
====================
Procedural Animation
====================
*/

float FadeOut(float timer, float duration);
float TextFloat(float currentOffset, float speed, float dt);
void DamageFloat(Vector2& pos, Vector2& vel, float gravity, float friction, float dt);
Vector2 LerpTowards(Vector2 current, Vector2 target, float speed, float dt);
bool Blink(float timer, float frequency);
float SlashShort(float raycastAngle, float progress, bool isRight);
float SlashMid(float raycastAngle, float progress, bool isRight);
float SlamShort(float raycastAngle, float progress, bool isRight);

/*
====================
Sound Effects System
====================
*/

void InitSFX();
void CloseSFX();
void PlaySFX(const std::string &name);