#include "animation.h"
#include "fonts.h"
#include "../lib/raylib/include/raymath.h"
#include "../lib/json/include/nlohmann/json.hpp"
#include <fstream>
#include <unordered_map>
#include <string>

using json = nlohmann::json;

/*
====================
Frames Management
====================
*/

/** @brief Array global texture */
Texture2D textures[MAX_TEXTURES];
/** @brief Cache frame dari JSON */
static std::unordered_map<std::string, Frame> loadedFrames;

/** @brief Map string ke TextureSlot */
Font fontKeybindHeader = {0};
Font fontKeybindEntry = {0};
Font fontLoadingTitle = {0};
static TextureSlot ResolveTextureSlot(const std::string &str)
{
    static const std::unordered_map<std::string, TextureSlot> mapping = {
        {"TILESET_MAP_1", TILESET_MAP_1},
        {"TILESET_MAP_2", TILESET_MAP_2},
        {"TILESET_PROPS", TILESET_PROPS},
        {"TILESET_ITEMS", TILESET_ITEMS},
        {"SPRITESHEET_KNIGHT", SPRITESHEET_KNIGHT},
        {"SPRITESHEET_ENEMIES", SPRITESHEET_ENEMIES},
        {"SPRITESHEET_EFFECTS", SPRITESHEET_EFFECTS}
    };
    auto it = mapping.find(str);
    if (it != mapping.end()) return it->second;
    return TILESET_MAP_1;
}

static void LoadFramesFromJSON()
{
    {
        std::ifstream file("assets/data/tiles.json");
        if (!file.is_open())
        {
            TraceLog(LOG_ERROR, "ANIMATION: Cannot open assets/data/tiles.json");
        }
        else
        {
            try
            {
                json root = json::parse(file);
                for (auto &[key, val] : root.items())
                {
                    Frame f;
                    f.texture = ResolveTextureSlot(val.at("texture").get<std::string>());
                    f.positionX = val.at("column").get<int>();
                    f.positionY = val.at("row").get<int>();
                    f.width = val.contains("width") ? val.at("width").get<int>() : 1;
                    f.height = val.contains("height") ? val.at("height").get<int>() : 1;
                    loadedFrames[key] = f;
                }
                TraceLog(LOG_INFO, "ANIMATION: Successfully loaded tiles.json");
            }
            catch (const std::exception &e)
            {
                TraceLog(LOG_ERROR, "ANIMATION: Error parsing tiles.json: %s", e.what());
            }
        }
    }

    {
        std::ifstream file("assets/data/sprites.json");
        if (!file.is_open())
        {
            TraceLog(LOG_ERROR, "ANIMATION: Cannot open assets/data/sprites.json");
        }
        else
        {
            try
            {
                json root = json::parse(file);
                for (auto &[key, val] : root.items())
                {
                    Frame f;
                    f.texture = ResolveTextureSlot(val.at("texture").get<std::string>());
                    f.positionX = val.at("column").get<int>();
                    f.positionY = val.at("row").get<int>();
                    f.width = val.contains("width") ? val.at("width").get<int>() : 1;
                    f.height = val.contains("height") ? val.at("height").get<int>() : 1;
                    loadedFrames[key] = f;
                }
                TraceLog(LOG_INFO, "ANIMATION: Successfully loaded sprites.json");
            }
            catch (const std::exception &e)
            {
                TraceLog(LOG_ERROR, "ANIMATION: Error parsing sprites.json: %s", e.what());
            }
        }
    }
}

void LoadFrameTexture(TextureSlot slot, const char *path)
{
    Image img = LoadImage(path);
    textures[slot] = LoadTextureFromImage(img);
    UnloadImage(img);
}

void InitTextures()
{
    LoadFrameTexture(TILESET_MAP_1, "assets/textures/tiles.png");
    LoadFrameTexture(TILESET_MAP_2, "assets/textures/test.png");
    LoadFrameTexture(TILESET_PROPS, "assets/textures/props.png");
    LoadFrameTexture(TILESET_ITEMS, "assets/textures/items.png");
    LoadFrameTexture(SPRITESHEET_KNIGHT, "assets/textures/knight (1).png");
    LoadFrameTexture(SPRITESHEET_ENEMIES, "assets/textures/enemies.png");
    LoadFrameTexture(SPRITESHEET_EFFECTS, "assets/textures/effects.png");
    LoadFramesFromJSON();
    LoadAnimationsFromJSON();
    InitFonts();
}

void InitFonts(void)
{
    static bool loaded = false;
    if (loaded) { TraceLog(LOG_INFO, "FONTS: Already loaded, skipping"); return; }
    loaded = true;

    const char *paths[] = { "assets/fonts/", "build/bin/assets/fonts/" };

    for (int pass = 0; pass < 2; pass++)
    {
        const char *dir = paths[pass];
        char buf[256];

        snprintf(buf, sizeof(buf), "%sNewDawn.ttf", dir);
        if (!FileExists(buf)) { TraceLog(LOG_WARNING, "FONTS: %s NOT FOUND", buf); continue; }
        fontKeybindHeader = LoadFontEx(buf, 32, 0, 0);
        break;
    }

    for (int pass = 0; pass < 2; pass++)
    {
        const char *dir = paths[pass];
        char buf[256];

        snprintf(buf, sizeof(buf), "%sPoppins-Regular.ttf", dir);
        if (!FileExists(buf)) { TraceLog(LOG_WARNING, "FONTS: %s NOT FOUND", buf); continue; }
        fontKeybindEntry = LoadFontEx(buf, 30, 0, 0);
        break;
    }

    for (int pass = 0; pass < 2; pass++)
    {
        const char *dir = paths[pass];
        char buf[256];

        snprintf(buf, sizeof(buf), "%sPoppins-Bold.ttf", dir);
        if (!FileExists(buf)) { TraceLog(LOG_WARNING, "FONTS: %s NOT FOUND", buf); continue; }
        fontLoadingTitle = LoadFontEx(buf, 32, 0, 0);
        break;
    }

    if (fontKeybindHeader.glyphCount == 0)
    {
        TraceLog(LOG_WARNING, "FONTS: NewDawn.ttf loaded but has 0 glyphs (using default font fallback)");
        fontKeybindHeader = GetFontDefault();
    }
    if (fontKeybindEntry.glyphCount == 0)
    {
        TraceLog(LOG_WARNING, "FONTS: Poppins-Regular.ttf loaded but has 0 glyphs (using default font fallback)");
        fontKeybindEntry = GetFontDefault();
    }
    if (fontLoadingTitle.glyphCount == 0)
    {
        TraceLog(LOG_WARNING, "FONTS: Poppins-Bold.ttf loaded but has 0 glyphs (using default font fallback)");
        fontLoadingTitle = GetFontDefault();
    }

    TraceLog(LOG_INFO, "FONTS: NewDawn (header) glyphs=%d, Poppins (entry) glyphs=%d, Poppins-Bold (loading) glyphs=%d",
        fontKeybindHeader.glyphCount, fontKeybindEntry.glyphCount, fontLoadingTitle.glyphCount);
}

void UnloadFonts(void)
{
    UnloadFont(fontKeybindHeader);
    UnloadFont(fontKeybindEntry);
    UnloadFont(fontLoadingTitle);
    TraceLog(LOG_INFO, "FONTS: Unloaded custom fonts");
}

void CloseTextures()
{
    for (int i = 0; i < MAX_TEXTURES; i++)
    {
        if (textures[i].id != 0)
        {
            UnloadTexture(textures[i]);
            textures[i] = {0};
        }
    }
}

const Frame &GetFrame(const std::string &id)
{
    auto it = loadedFrames.find(id);
    if (it != loadedFrames.end())
    {
        return it->second;
    }
    
    TraceLog(LOG_WARNING, "ANIMATION: Frame not found: %s", id.c_str());
    static const Frame fallback = { TILESET_MAP_1, 0, 0, 1, 1 };
    return fallback;
}

void DrawFrame(Frame frame, Display display)
{
    Rectangle src = {
        (float)(frame.positionX * (FRAME_SIZE + FRAME_GAP)),
        (float)(frame.positionY * (FRAME_SIZE + FRAME_GAP)),
        (float)(frame.width * FRAME_SIZE),
        (float)(frame.height * FRAME_SIZE)
    };
    Rectangle dest = {
        display.position.x + display.offset.x,
        display.position.y + display.offset.y,
        (float)(frame.width * display.size),
        (float)(frame.height * display.size)
    };
    DrawTexturePro(textures[frame.texture], src, dest, display.origin, display.rotation, display.tint);
}

void DrawFrame(const std::string &id, Display display)
{
    DrawFrame(GetFrame(id), display);
}

/*
====================
Sprites Animation
====================
*/

std::unordered_map<std::string, AnimationSet> loadedAnimationSets;

static State ResolveState(const std::string &str)
{
    static const std::unordered_map<std::string, State> mapping = {
        {"idle", IDLE},
        {"walk", WALK},
        {"attack", ATTACK},
        {"dead", DEAD}
    };
    auto it = mapping.find(str);
    if (it != mapping.end()) return it->second;
    return IDLE;
}

static Direction ResolveDirection(const std::string &str)
{
    static const std::unordered_map<std::string, Direction> mapping = {
        {"left", LEFT},
        {"right", RIGHT},
        {"down", DOWN},
        {"up", UP}
    };
    auto it = mapping.find(str);
    if (it != mapping.end()) return it->second;
    return RIGHT;
}

void LoadAnimationsFromJSON()
{
    std::ifstream file("assets/data/animations.json");
    if (!file.is_open())
    {
        TraceLog(LOG_ERROR, "ANIMATION: Gagal membuka assets/data/animations.json");
        return;
    }

    try
    {
        json root = json::parse(file);
        for (auto &[entityKey, entityVal] : root.items())
        {
            AnimationSet set;
            for (int s = 0; s < 4; s++) {
                for (int d = 0; d < 4; d++) {
                    set.configs[s][d].speed = 0.5f;
                    set.configs[s][d].loop = true;
                }
            }

            for (auto &[stateKey, stateVal] : entityVal.items())
            {
                State state = ResolveState(stateKey);
                
                for (auto &[dirKey, dirVal] : stateVal.items())
                {
                    Direction dir = ResolveDirection(dirKey);
                    
                    AnimationConfig config;
                    config.speed = dirVal.at("spriteDuration").get<float>();
                    config.loop = dirVal.at("loop").get<bool>();
                    
                    if (dirVal.contains("sprites"))
                    {
                        for (auto &sprite : dirVal.at("sprites"))
                        {
                            config.sprites.push_back(sprite.get<std::string>());
                        }
                    }
                    
                    set.configs[state][dir] = config;
                }
                
            }
            loadedAnimationSets[entityKey] = set;
        }
        TraceLog(LOG_INFO, "ANIMATION: Berhasil memuat %d animation sets dari JSON", (int)loadedAnimationSets.size());
    }
    catch (const std::exception &e)
    {
        TraceLog(LOG_ERROR, "ANIMATION: Gagal parse animations.json: %s", e.what());
    }
}

void PlayAnimation(Animation &anim, State state, Direction direction)
{
    if (!anim.animSet) return;

    Direction resolvedDir = direction;
    
    if (anim.animSet->configs[state][resolvedDir].sprites.empty())
    {
        if (resolvedDir == UP || resolvedDir == DOWN)
        {
            bool wasLeft = false;
            bool wasRight = false;
            
            if (anim.currentConfig)
            {
                for (int s = 0; s < 4; s++) {
                    if (anim.currentConfig == &anim.animSet->configs[s][LEFT]) wasLeft = true;
                    if (anim.currentConfig == &anim.animSet->configs[s][RIGHT]) wasRight = true;
                }
            }
            
            if (wasLeft) resolvedDir = LEFT;
            else if (wasRight) resolvedDir = RIGHT;
            else if (anim.direction == LEFT || anim.direction == RIGHT) resolvedDir = anim.direction;
            else resolvedDir = RIGHT;
        }
        
        if (anim.animSet->configs[state][resolvedDir].sprites.empty())
        {
            if (!anim.animSet->configs[state][RIGHT].sprites.empty()) resolvedDir = RIGHT;
            else if (!anim.animSet->configs[state][LEFT].sprites.empty()) resolvedDir = LEFT;
        }
    }

    if (anim.state == state && anim.direction == direction && anim.currentConfig == &anim.animSet->configs[state][resolvedDir]) return;

    anim.state = state;
    anim.direction = direction;
    anim.currentConfig = &anim.animSet->configs[state][resolvedDir];
    
    anim.timer = 0.0f;
    anim.currentSpriteIndex = 0;
}

void UpdateAnimation(Animation &anim, float dt)
{
    if (!anim.currentConfig || anim.currentConfig->sprites.empty()) return;

    anim.timer += dt;
    if (anim.timer >= anim.currentConfig->speed)
    {
        anim.timer = 0.0f;
        anim.currentSpriteIndex++;

        if (anim.currentSpriteIndex >= (int)anim.currentConfig->sprites.size())
        {
            if (anim.currentConfig->loop)
            {
                anim.currentSpriteIndex = 0;
            }
            else
            {
                anim.currentSpriteIndex = (int)anim.currentConfig->sprites.size() - 1;
                
                if (anim.state == ATTACK)
                {
                    anim.isAttacking = false;
                    PlayAnimation(anim, IDLE, anim.direction);
                }
            }
        }
    }
}

void DrawAnimation(const Animation &anim, Color tint)
{
    if (!anim.currentConfig || anim.currentConfig->sprites.empty()) return;

    int index = anim.currentSpriteIndex;
    if (index < 0 || index >= (int)anim.currentConfig->sprites.size())
    {
        index = 0;
    }

    const std::string &frameId = anim.currentConfig->sprites[index];
    Display display = { anim.position, FRAME_SIZE, {0,0}, {0,0}, 0.0f, tint };
    DrawFrame(frameId, display);
}

void Explosion(Vector2 centerPosition, float radius, float progress)
{
    std::string frameName;
    auto it = loadedAnimationSets.find("explosion");
    if (it != loadedAnimationSets.end())
    {
        const AnimationConfig &config = it->second.configs[IDLE][RIGHT];
        if (!config.sprites.empty())
        {
            int frameIndex = (int)(progress * config.sprites.size());
            if (frameIndex < 0) frameIndex = 0;
            if (frameIndex >= (int)config.sprites.size()) frameIndex = (int)config.sprites.size() - 1;
            frameName = config.sprites[frameIndex];
        }
    }
    if (frameName.empty())
    {
        int frameIndex = (int)(progress * 10.0f);
        if (frameIndex < 0) frameIndex = 0;
        if (frameIndex > 9) frameIndex = 9;
        frameName = "explosion" + std::to_string(frameIndex + 1);
    }

    Display display;
    display.size = (int)(radius * 2.0f / 3.0f);
    display.position.x = centerPosition.x - (3.0f * display.size) / 2.0f;
    display.position.y = centerPosition.y - (3.0f * display.size) / 2.0f;
    display.offset = {0, 0};

    DrawFrame(frameName, display);
}

/*
====================
Procedural Animation
====================
*/

float FadeOut(float timer, float duration)
{
    if (duration <= 0.0f) return 0.0f;
    float alpha = 1.0f - (timer / duration);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return alpha;
}

float TextFloat(float currentOffset, float speed, float dt)
{
    return currentOffset - (speed * dt);
}

void DamageFloat(Vector2& pos, Vector2& vel, float gravity, float friction, float dt)
{
    pos = Vector2Add(pos, Vector2Scale(vel, dt));
    vel.y += gravity;
    vel.x *= friction;
}

Vector2 LerpTowards(Vector2 current, Vector2 target, float speed, float dt)
{
    Vector2 dir = Vector2Normalize(Vector2Subtract(target, current));
    return Vector2Add(current, Vector2Scale(dir, speed * dt));
}

bool Blink(float timer, float frequency)
{
    if (frequency <= 0.0f) return true;
    return (int)(timer * frequency * 10.0f) % 2 == 0;
}

float Slash(float raycastAngle, float progress)
{
    if (progress < 1.0f / 4.0f)
        return raycastAngle + 90.0f;
    else if (progress < 2.0f / 4.0f)
        return raycastAngle - 30.0f;
    else
        return raycastAngle - 90.0f;
}
