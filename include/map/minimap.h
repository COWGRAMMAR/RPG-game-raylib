#pragma once

/**
 * @file minimap.h
 * @brief Minimap navigation aid — tile grid + fog of war + player marker.
 *
 * Bukan wallhack: cuma nampilin layout statis (wall/floor) dan posisi player.
 * gak ada enemy / item / runtime object dots.
 *
 * Grid texture dibuild di MINIMAP_TILE_PX px per tile (high-res),
 * fog di render texture resolusi sama, DrawTexturePro dengan PANEL_SCALE.
 */

#include "raylib.h"
#include "screen.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

/*==============================================================================
 * Minimap Constants
 *==============================================================================*/

/// 1 tile map = 1 px pada minimap grid
constexpr int MINIMAP_TILE_TO_PX   = 1;
/// Pixels per tile in grid texture (detail level — build constant)
constexpr int MINIMAP_TILE_PX     = 16;
/// Fixed zoom multiplier (no dynamic zoom, float) — PANEL_SCALE = TILE_PX * ZOOM
constexpr float MINIMAP_ZOOM      = 0.7f;
/// Radius reveal fog of war (dalam tile map)
constexpr int MINIMAP_REVEAL_RADIUS = 10;
/// Warna dasar fog (beige) — UNEXPLORED & EXPLORED pake warna ini
#define FOG_COLOR_BEIGE 255, 215, 152, 255
/// Scale factor pas render grid ke layar (TILE_PX * ZOOM pixel per tile)
constexpr float MINIMAP_PANEL_SCALE = (float)MINIMAP_TILE_PX * MINIMAP_ZOOM;
/// Padding dalam panel (px)
constexpr int MINIMAP_PANEL_PADDING = 10;
/// Radius player marker (screen pixels) — independen dari PANEL_SCALE
constexpr float MINIMAP_MARKER_RADIUS = 4.0f;

/// Ukuran tetap viewport minimap (screen pixels)
constexpr int MINIMAP_VIEWPORT_WIDTH  = 500;
constexpr int MINIMAP_VIEWPORT_HEIGHT = 460;

/*==============================================================================
 * FogState Enum
 *==============================================================================*/

enum class FogState : unsigned char
{
    UNEXPLORED = 0,  ///< Belum pernah dikunjungi → hitam pekat
    VISIBLE    = 1,  ///< Lagi kelihatan → transparan
    EXPLORED   = 2   ///< Pernah kelihatan → semi-gelap
};

/*==============================================================================
 * MinimapData — Tile Grid + Fog Runtime
 *==============================================================================*/

/**
 * @brief Data inti minimap: grid dimensi + fog array.
 *
 * Grid warna tidak disimpan — langsung sampling tilesonMap tiap frame.
 * fogCache nyimpen fog per map path buat persistensi (mirip barrierMap).
 */
struct MinimapData
{
    int gridWidth   = 0;  ///< mapTileWidth  / MINIMAP_TILE_TO_PX
    int gridHeight  = 0;  ///< mapTileHeight / MINIMAP_TILE_TO_PX
    int mapTileWidth  = 0;  ///< Ukuran asli map (tile)
    int mapTileHeight = 0;

    /// Fog state per grid cell — 0/1/2, row-major
    std::vector<unsigned char> fog;

    /// Cache fog per map path — persistensi lintas map switch
    std::unordered_map<std::string, std::vector<unsigned char>> fogCache;
};



/*==============================================================================
 * Fog Functions
 *==============================================================================*/

/**
 * @brief Reset fog → UNEXPLORED semua.
 */
void ResetMinimapFog();

/**
 * @brief Update fog dari posisi player tiap frame.
 *
 * @param worldX    Posisi player X (dalam pixel)
 * @param worldY    Posisi player Y (dalam pixel)
 * @param tileSize  Konstanta TILE_SIZE (biasanya 16)
 */
void UpdateMinimapFog(float worldX, float worldY, int tileSize);

/**
 * @brief Simpan fog current map ke fogCache.
 * @param mapPath  Key unik map (path .tmj atau path+seed untuk worldgen)
 */
void SaveMinimapFogToCache(const std::string& mapPath);

/**
 * @brief Restore fog dari fogCache untuk map tertentu.
 * @param mapPath  Key unik map
 */
void RestoreMinimapFogFromCache(const std::string& mapPath);

/*==============================================================================
 * MinimapScreen — Overlay Screen
 *==============================================================================*/

/**
 * @class MinimapScreen
 * @brief Overlay minimap — toggle on/off, game pause di belakang.
 *
 * Pola: Show()/Hide()/IsActive()/Update()/Draw()
 * Sama kayak OptionsScreen / PauseMenu.
 */
class MinimapScreen
{
public:
    MinimapScreen();
    ~MinimapScreen();

    /** @brief Init render texture dan state awal */
    void Init();

    /** @brief Unload render texture */
    void Shutdown();

    /** @brief Tampilkan minimap overlay */
    void Show();

    /** @brief Sembunyikan minimap overlay */
    void Hide();

    /** @return true kalo minimap sedang aktif */
    bool IsActive() const;

    /** @brief Update input & fog tiap frame */
    void Update(GameState* state, Vector2 mousePosition, bool mouseClicked);

    /** @brief Render minimap overlay */
    void Draw(Vector2 mousePosition);

private:
    /** @brief Build grid texture dari tilesonMap + collision (panggil sekali pas Init) */
    void BuildGridTexture();

    /** @brief Render full map fog ke fogRT (ClearBackground + semua tile) — panggil sekali pas Init */
    void RenderFogFull();

    /** @brief Update fogRT cuma untuk tile yang visible di viewport — panggil tiap frame di Update */
    void RenderFogVisible();

    /** @brief Gambar fog RT ke panel */
    void DrawFogLayer() const;

    /** @brief Gambar marker player di atas grid+fog */
    void DrawPlayerMarker() const;

    /** @brief Update panOffset agar view selalu center ke player (tiap frame) */
    void UpdateView();

    /** @brief Handle drag untuk pan */
    void HandlePan(Vector2 mousePosition, bool mouseClicked);

    /** @brief Gambar legend (close hint + drag/center hints) */
    void DrawLegend() const;

    /** @brief Hitung layout panel berdasarkan grid size */
    void CalculateLayout();

    /*----------------------------------------------------------------------
     * State
     *----------------------------------------------------------------------*/

    bool active;        ///< Sedang tampil?
    bool initialized;   ///< Init() udah dipanggil?

    /*----------------------------------------------------------------------
     * Textures
     *----------------------------------------------------------------------*/

    Texture2D gridTexture;    ///< Static grid (build sekali dari tilesonMap)
    RenderTexture2D fogRT;    ///< Fog render texture (full resolution, per-tile fog drawing)
    Texture2D bgArtwork;      ///< Panel background decoration (mapBG.png)

    /*----------------------------------------------------------------------
     * Fog Render Shader — state sampling + circular reveal
     *----------------------------------------------------------------------*/

    Shader fogRenderShader = {0};  ///< Fragment shader: state→beige mapping + circle mask
    int fogCenterLoc = -1;         ///< Uniform: circleCenter (vec2, texel coords)
    int fogRadiusLoc = -1;         ///< Uniform: circleRadius (float, texel radius)

    /*----------------------------------------------------------------------
     * Pan (manual offset, gak pake Camera2D)
     *----------------------------------------------------------------------*/

    Vector2 panOffset;   ///< Offset panning dalam screen pixels
    Vector2 dragStart;
    bool isDragging;
    bool followPlayer;   ///< Auto-follow player (dijeda pas manual drag)

    /*----------------------------------------------------------------------
     * Layout (screen coordinates, virtual 640x360)
     *----------------------------------------------------------------------*/

    Rectangle panelRect;  ///< Background panel bounds
    Rectangle viewRect;   ///< Area minimap di dalam panel (tempat grid+fog digambar)
};

/*==============================================================================
 * Globals
 *==============================================================================*/

/// Global minimap data (grid + fog)
extern MinimapData g_Minimap;

/// Global minimap screen instance
extern MinimapScreen g_MinimapScreen;

/*==============================================================================
 * MinimapSystem — Public Namespace Interface
 *==============================================================================*/

/**
 * @brief Public API untuk integrasi minimap ke game loop.
 *
 * External files cukup panggil 4 fungsi ini — no scattered minimap logic.
 */
namespace MinimapSystem {

    /**
     * @brief Init/rebuild minimap dari map yang sedang aktif.
     *
     * Panggil setelah LoadMap + SpawnObject + RebuildObstacleCache.
     * Otomatis: build grid → restore fog cache → init RenderTexture.
     */
    void InitWithMap();

    /**
     * @brief Shutdown minimap — simpan fog + unload RT.
     */
    void Shutdown();

    /**
     * @brief Update fog + handle toggle map.
     *
     * Panggil tiap frame di PLAY state setelah InputInstance.PollInput().
     */
    void Update();

    /**
     * @brief Draw minimap overlay (jika aktif).
     *
     * Panggil di dalam BeginTextureMode / DrawUIOverlay.
     */
    void Draw();

} // namespace MinimapSystem
