#pragma once

/**
 * @file maplogic.h
 * @brief Map Logic, Collision, dan Raycasting Helper Module
 *
 * Modul ini nyediain semua utility yang berhubungan sama interaksi map:
 * - Query object dari Tiled (by name, type, layer)
 * - Collision detection (rectangle & polygon)
 * - Raycasting (ray vs rect, ray vs polygon)
 * - Hitbox utilities
 * - World boundary check
 */

#include "../lib/raylib/include/raylib.h"
#include "../lib/tileson/tileson.hpp"
#include "map.h"
#include <vector>
#include <string>
#include <optional>


/*==============================================================================
 * MapObjectIndex
 *==============================================================================*/

/** @brief Bangun index object untuk target map */
void BuildMapObjectIndexTarget(TilesonMapData *target);
/** @brief Bangun index object untuk map aktif */
void BuildMapObjectIndex();

/*==============================================================================
 * Object Query Functions
 *==============================================================================*/

/**
 * @brief Dapetin semua object dari layer tertentu
 * @param layerName Nama layer di Tiled
 * @return const reference ke internal vector — zero copy
 */
const std::vector<MapObject *> &TilesonGetObjectsByLayerName(const std::string &layerName);

/**
 * @brief Dapetin semua object dengan type tertentu
 * @param type Nilai field Type di Tiled
 * @return const reference ke internal vector — zero copy
 */
const std::vector<MapObject *> &TilesonGetObjectsByType(const std::string &type);

/**
 * @brief Dapetin object berdasarkan nama
 * @param name Nama object di Tiled
 * @return Pointer ke MapObject kalo ketemu, nullptr kalo tidak ada
 */
MapObject *TilesonGetObjectByName(const std::string &name);

/*==============================================================================
 * TiledHelper
 *==============================================================================*/

/**
 * @brief Helper class untuk mengakses data object dari Tiled
 *
 * Semua method bersifat static — tidak perlu instansiasi manual.
 * Gunakan global instance TiledHelperFunction untuk akses dari luar.
 *
 * Tanggung jawab:
 * - Lookup posisi object by name
 * - Ekstrak collision data by layer
 */
class TiledHelper
{
public:
    // =============================================
    // Position Helpers
    // =============================================

    /**
     * @brief Coba dapetin posisi object berdasarkan namanya
     * @param objectName Nama object di Tiled
     * @param outPosition [OUT] Posisi object kalo ketemu
     * @return true kalo object ditemukan
     */
    static bool TryGetObjectPositionByName(const std::string &objectName, Vector2 &outPosition);

    // =============================================
    // Collision Helpers
    // =============================================

    /**
     * @brief Hasil query collision — berisi rects dan polygons terpisah
     */
    struct CollisionResult
    {
        std::vector<Rectangle> rects;               // rectangle collision
        std::vector<std::vector<Vector2>> polygons; // polygon collision
    };

    /**
     * @brief Coba dapetin collision data berdasarkan nama layer
     * @param layerName Nama layer di Tiled (misal "obstacle")
     * @param outCollision [OUT] CollisionResult berisi rects dan polygons
     * @return true kalo layer ditemukan dan ada data collision
     */
    static bool TryGetCollisionByLayerName(const std::string &layerName, CollisionResult &outCollision);

    /**
     * @brief Dapetin semua object berdasarkan typenya
     * @param objectType Type object di Tiled
     * @return Vector pointer ke MapObject yang typenya sesuai
     */
    static std::vector<MapObject *> GetObjectsByType(const std::string &objectType);
};

/** @brief Instance global TiledHelper */
extern TiledHelper TiledHelperFunction;

/*==============================================================================
 * Collision Cache
 *==============================================================================*/

/** @brief Cache collision global */
extern TiledHelper::CollisionResult gCollisionCache;
/** @brief Rebuild collision cache */
void RebuildCollisionCache();

/*==============================================================================
 * RayCast
 *==============================================================================*/

/**
 * @brief Hasil dari satu ray cast
 */
struct RayHitResult
{
    bool hit;          // true kalo ray kena sesuatu
    Vector2 point;     // titik tumbukan di world space
    float distance;    // jarak dari origin ke titik tumbukan (pixel)
    MapObject *object; // pointer ke object yang kena, nullptr kalo miss
};

/**
 * @brief Modular raycasting system — bisa dipakai oleh sistem manapun
 *
 * RayCast tidak tahu soal player atau game state.
 * Caller yang tanggung jawab ngasih origin, direction, maxDistance, dan list object.
 *
 * Supported shapes:
 * - Rectangle (via HitRect)
 * - Polygon (via HitPolygon)
 *
 * Dependency:
 * - GetRectangleCorners() untuk decompose rect jadi edges
 * - IsPointInPolygon() untuk early-out kalo origin di dalam polygon
 */
class RayCast
{
public:
    /**
     * @brief Cast satu ray dan cek terhadap semua object yang diberikan
     * @param origin Titik asal ray (world space)
     * @param direction Arah ray — harus normalized
     * @param maxDistance Panjang maksimum ray (pixel)
     * @param objects List object yang mau dicek
     * @return RayHitResult dengan data hit terdekat, atau hit=false kalo miss semua
     */
    RayHitResult Cast(Vector2 origin, Vector2 direction,
                      float maxDistance, std::vector<MapObject> &objects);

    /**
     * @brief Cast multiple rays dalam bentuk cone dan return hit terdekat
     * @param origin Titik asal cone (world space)
     * @param forward Arah hadap — harus normalized
     * @param maxDistance Panjang maksimum tiap ray (pixel)
     * @param halfAngleDeg Setengah lebar cone dalam derajat (misal 45.0f = cone 90°)
     * @param rayCount Jumlah ray yang ditembak dalam cone
     * @param objects List object yang mau dicek
     * @return RayHitResult dengan data hit terdekat, atau hit=false kalo semua ray miss
     */
    RayHitResult CastCone(Vector2 origin, Vector2 forward, float maxDistance,
                          float halfAngleDeg, int rayCount, std::vector<MapObject> &objects);

private:
    // cek ray vs rectangle — decompose jadi 4 edge lalu cek tiap edge
    std::optional<float> HitRect(Vector2 origin, Vector2 direction, Rectangle rect, float maxDistance);

    // cek ray vs polygon — loop tiap edge, early-out kalo origin di dalam polygon
    std::optional<float> HitPolygon(Vector2 origin, Vector2 direction, std::vector<Vector2> &polygon, float maxDistance);

    // hitung intersection dua line segment — return jarak pixel kalo intersect, nullopt kalo tidak
    std::optional<float> LineIntersect(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4);
};

/*==============================================================================
 * Hitbox Utilities
 *==============================================================================*/

/**
 * @brief Bikin hitbox rectangle dari posisi dan offset
 * @param position Posisi dasar entity
 * @param offsetX Offset X dari posisi
 * @param offsetY Offset Y dari posisi
 * @param width Lebar hitbox
 * @param height Tinggi hitbox
 * @return Rectangle hitbox yang sudah di-offset
 */
Rectangle BuildHitbox(Vector2 position, float offsetX, float offsetY, float width, float height);

/**
 * @brief Dapetin 4 titik sudut dari rectangle
 * @param rect Rectangle sumber
 * @return Array 4 Vector2: [0] TL, [1] TR, [2] BL, [3] BR
 */
std::array<Vector2, 4> GetRectangleCorners(const Rectangle &rect);

/*==============================================================================
 * Point-in-Polygon
 *==============================================================================*/

/**
 * @brief Cek apakah sebuah titik berada di dalam polygon
 * @param point Titik yang mau dicek
 * @param polygon Titik-titik polygon berurutan (min 3 titik)
 * @return true kalo titik di dalam polygon
 * @note Menggunakan even-odd rule (ray casting algorithm)
 */
bool IsPointInPolygon(Vector2 point, const std::vector<Vector2> &polygon);

/*==============================================================================
 * Collision Check Helpers
 *==============================================================================*/

/**
 * @brief Cek collision hitbox terhadap kumpulan rectangle
 * @param hitbox Hitbox entity
 * @param collisionRects Daftar rectangle collision dari map
 * @return true kalo ada tabrakan
 */
bool CheckCollisionAgainstRects(const Rectangle &hitbox, const std::vector<Rectangle> &collisionRects);

/**
 * @brief Cek collision hitbox terhadap kumpulan polygon
 * @param hitbox Hitbox entity
 * @param collisionPolygons Daftar polygon collision dari map
 * @return true kalo ada tabrakan
 */
bool CheckCollisionAgainstPolygons(const Rectangle &hitbox, const std::vector<std::vector<Vector2>> &collisionPolygons);

/*==============================================================================
 * World Boundary
 *==============================================================================*/

/**
 * @brief Cek apakah hitbox masih di dalam batas dunia
 * @param hitbox Hitbox entity
 * @param worldWidth Lebar dunia dalam pixel
 * @param worldHeight Tinggi dunia dalam pixel
 * @return true kalo hitbox di dalam batas
 */
bool IsWithinWorldBounds(const Rectangle &hitbox, float worldWidth, float worldHeight);

/**
 * @brief Cek apakah sebuah posisi aman (tidak menabrak wall/collision) dan berada di dalam batas map
 * @param pos Posisi yang mau dicek
 * @param width Lebar hitbox
 * @param height Tinggi hitbox
 * @param offsetX Offset X hitbox
 * @param offsetY Offset Y hitbox
 * @return true kalo posisi aman untuk spawn/gerak
 */
bool IsPositionSafe(Vector2 pos, float width, float height, float offsetX, float offsetY);

/*==============================================================================
 * Dynamic Obstacles
 *==============================================================================*/

/** @brief Daftar obstacle dinamis */
extern std::vector<Rectangle> DynamicObstacles;