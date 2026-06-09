#pragma once
#include "../lib/raylib/include/raylib.h"
#include <string>
#include <vector>
#include <unordered_map>

/** @brief Frame Management */
/** @brief Ukuran tile/frame dalam pixel */
constexpr int FRAME_SIZE = 32;
/** @brief Gap antar frame di spritesheet */
constexpr int FRAME_GAP = 4;
/** @brief Jumlah maksimum texture slot */
constexpr int MAX_TEXTURES = 7;

/** @brief Array global texture */
extern Texture2D textures[MAX_TEXTURES];

/** @brief Slot index untuk texture lookup */
enum TextureSlot
{
    TILESET_MAP_1,      // Slot tileset map 1
    TILESET_MAP_2,      // Slot tileset map 2
    TILESET_PROPS,      // Slot tileset props
    TILESET_ITEMS,      // Slot tileset items
    SPRITESHEET_KNIGHT, // Slot spritesheet knight
    SPRITESHEET_ENEMIES,// Slot spritesheet enemies
    SPRITESHEET_EFFECTS // Slot spritesheet effects
};

/** @brief Referensi ke satu frame dalam spritesheet */
struct Frame
{
    TextureSlot texture; // Texture slot yang digunakan
    int positionX;       // Posisi X dalam spritesheet
    int positionY;       // Posisi Y dalam spritesheet
    int width = 1;       // Lebar frame (dalam tile unit)
    int height = 1;      // Tinggi frame (dalam tile unit)
};

/** @brief Informasi rendering untuk satu frame */
struct Display
{
    Vector2 position;    // Posisi world space
    int size = FRAME_SIZE; // Ukuran render (pixel)
    Vector2 offset = {0, 0}; // Offset posisi
    Vector2 origin = {0, 0}; // Origin rotasi
    float rotation = 0.0f;   // Rotasi (derajat)
    Color tint = WHITE;      // Warna tint
};

/** @brief Load texture dari file ke slot tertentu */
void LoadFrameTexture(TextureSlot slot, const char *path);
/** @brief Init semua texture yang dibutuhkan */
void InitTextures();
/** @brief Unload semua texture */
void CloseTextures();
/** @brief Ambil frame reference berdasarkan ID string */
const Frame &GetFrame(const std::string &id);
/** @brief Draw frame dengan display info */
void DrawFrame(Frame frame, Display display);
/** @brief Draw frame berdasarkan ID string */
void DrawFrame(const std::string &id, Display display);

/** @brief Sprite Animation (State Machine) */

/** @brief State animasi entity */
enum State
{
    IDLE,  // Idle / diam
    WALK,  // Berjalan
    ATTACK,// Menyerang
    DEAD   // Mati
};

/** @brief Arah hadap entity */
enum Direction
{
    LEFT,  // Menghadap kiri
    RIGHT, // Menghadap kanan
    DOWN,  // Menghadap bawah
    UP     // Menghadap atas
};

/** @brief Konfigurasi satu state animasi */
struct AnimationConfig
{
    std::vector<std::string> sprites; // Daftar ID frame untuk state ini
    float speed;                      // Kecepatan animasi (detik per frame)
    bool loop;                        // Apakah animasi looping
};

/** @brief Kumpulan konfigurasi animasi untuk satu entity */
struct AnimationSet
{
    AnimationConfig configs[4][4]; // [State][Direction] -> konfigurasi animasi
};

/** @brief Runtime state animasi */
struct Animation
{
    Vector2 position;            // Posisi world entity
    State state;                 // State saat ini
    Direction direction;         // Arah hadap saat ini
    float timer;                 // Timer animasi (detik)
    int currentSpriteIndex;      // Index frame sprite saat ini
    bool isAttacking;            // Apakah sedang attack
    bool isDead;                 // Apakah entity sudah mati
    const AnimationConfig *currentConfig; // Pointer ke konfigurasi aktif
    const AnimationSet *animSet; // Pointer ke animation set entity
};

/** @brief Load semua animasi dari file JSON */
void LoadAnimationsFromJSON();
/** @brief Ganti state animasi dan reset timer */
void PlayAnimation(Animation &anim, State state, Direction direction);
/** @brief Update timer dan sprite index animasi */
void UpdateAnimation(Animation &anim, float dt);
/** @brief Draw animasi di posisi entity */
void DrawAnimation(const Animation &anim, Color tint = WHITE);
/** @brief Draw efek ledakan sementara */
void Explosion(Vector2 centerPosition, float radius, float progress);

/** @brief Cache global animation set per entity type */
extern std::unordered_map<std::string, AnimationSet> loadedAnimationSets;

/** @brief Procedural Animation Helpers */

/** @brief Interpolasi fade-out (0->1) berdasarkan timer */
float FadeOut(float timer, float duration);
/** @brief Animasi floating teks (damage, pickup) */
float TextFloat(float currentOffset, float speed, float dt);
/** @brief Update posisi damage number dengan gravity */
void DamageFloat(Vector2& pos, Vector2& vel, float gravity, float friction, float dt);
/** @brief Lerp posisi menuju target dengan kecepatan tetap */
Vector2 LerpTowards(Vector2 current, Vector2 target, float speed, float dt);
/** @brief Blink effect berdasarkan timer dan frekuensi */
bool Blink(float timer, float frequency);
/** @brief Hitung panjang garis slash berdasarkan progress */
float Slash(float raycastAngle, float progress);