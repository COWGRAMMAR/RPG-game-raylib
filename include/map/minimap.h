#pragma once

/**
 * @file minimap.h
 * @brief Minimap navigation aid — tile grid + fog of war + player marker.
 *
 * Bukan wallhack: cuma nampilin layout statis (wall/floor) dan posisi player.
 * gak ada enemy / item / runtime object dots.
 *
 * Scale: 2×2 tile → 1 px minimap grid.
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

/// 2 tile map = 1 px pada minimap grid
constexpr int MINIMAP_TILE_TO_PX   = 2;
/// Radius reveal fog of war (dalam tile map)
constexpr int MINIMAP_REVEAL_RADIUS = 10;
/// Alpha untuk fog explored (pernah visible, sekarang gelap lagi)
constexpr float MINIMAP_FOG_EXPLORED_ALPHA = 0.6f;
/// Scale factor pas render grid ke layar (1 grid px = N screen px)
constexpr int MINIMAP_PANEL_SCALE = 3;
/// Padding dalam panel (px)
constexpr int MINIMAP_PANEL_PADDING = 10;

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
 * @brief Data inti minimap: grid warna + fog array.
 *
 * Dibangun sekali pas map load via BuildMinimapGrid().
 * fogCache nyimpen fog per map path buat persistensi (mirip barrierMap).
 */
struct MinimapData
{
    int gridWidth   = 0;  ///< mapTileWidth  / MINIMAP_TILE_TO_PX
    int gridHeight  = 0;  ///< mapTileHeight / MINIMAP_TILE_TO_PX
    int mapTileWidth  = 0;  ///< Ukuran asli map (tile)
    int mapTileHeight = 0;

    /// Warna per grid cell — 1 Color per 2×2 tile block, row-major
    std::vector<Color> grid;

    /// Fog state per grid cell — 0/1/2, row-major
    std::vector<unsigned char> fog;

    /// Cache fog per map path — persistensi lintas map switch
    std::unordered_map<std::string, std::vector<unsigned char>> fogCache;
};

/*==============================================================================
 * Grid Building Functions
 *==============================================================================*/

/**
 * @brief Bangun minimap grid dari tile GID + obstacle rects.
 *
 * @param mapTileW       Lebar map dalam tile
 * @param mapTileH       Tinggi map dalam tile
 * @param tileGids       Array GID per tile (row-major, size = mapTileW * mapTileH)
 * @param obstacleRects  Rectangle dari Tiled object layer "obstacle" (dalam tile coords)
 */
void BuildMinimapGrid(int mapTileW, int mapTileH,
                      const std::vector<int>& tileGids,
                      const std::vector<Rectangle>& obstacleRects);

/**
 * @brief Petakan GID tile ke warna minimap.
 *
 * Sementara pake heuristic sederhana.
 * Nanti bisa diperhalus pas tileset properti udah jelas.
 */
Color MapGidToColor(int gid);

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
    /** @brief Render static grid ke gridRT (panggil sekali pas Init) */
    void PreRenderGrid();

    /** @brief Render fog layer ke fogRT (panggil tiap frame) */
    void RenderFogLayer();

    /** @brief Gambar fog RT ke panel */
    void DrawFogLayer() const;

    /** @brief Gambar marker player di atas grid+fog */
    void DrawPlayerMarker() const;

    /** @brief Handle drag untuk pan */
    void HandlePan(Vector2 mousePosition, bool mouseClicked);

    /** @brief Hitung layout panel berdasarkan grid size */
    void CalculateLayout();

    /*----------------------------------------------------------------------
     * State
     *----------------------------------------------------------------------*/

    bool active;        ///< Sedang tampil?
    bool initialized;   ///< Init() udah dipanggil?

    /*----------------------------------------------------------------------
     * Render Textures
     *----------------------------------------------------------------------*/

    RenderTexture2D gridRT;  ///< Static grid (pre-render sekali)
    RenderTexture2D fogRT;   ///< Fog layer (update tiap frame)

    /*----------------------------------------------------------------------
     * Pan (manual offset, gak pake Camera2D)
     *----------------------------------------------------------------------*/

    Vector2 panOffset;   ///< Offset panning dalam screen pixels
    Vector2 dragStart;
    bool isDragging;

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
