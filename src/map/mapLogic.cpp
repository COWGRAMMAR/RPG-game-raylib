/**
 * @file mapLogic.cpp
 * @brief Implementasi dari Map Logic, Collision, dan Raycasting Helper Module
 *
 * File ini berisi implementasi helper untuk:
 * - Lookup object dari data Tiled
 * - Build index object untuk query cepat
 * - Hitbox utility
 * - Collision check rectangle dan polygon
 * - Raycasting terhadap object map
 */

#include "mapLogic.h"
#include "../lib/raylib/include/raylib.h"
#include "../lib/raylib/include/raymath.h"
#include "../lib/tileson/tileson.hpp"
#include "map.h"
#include "animation.h"

/*==============================================================================
 * Global Variables
 *==============================================================================*/

TiledHelper TiledHelperFunction;

/* Collision cache — dibangun ulang tiap object index berubah */
TiledHelper::CollisionResult gCollisionCache;

/**
 * @brief Index object Tiled yang dipakai untuk lookup cepat
 *
 * Dibangun ulang lewat BuildMapObjectIndex() setelah map selesai dimuat.
 * Seluruh pointer mengarah langsung ke data object aktif di tilesonMap->Objects.
 */

/** @brief Bangun index object untuk target map */
void BuildMapObjectIndexTarget(TilesonMapData *target)
{
    target->objectIndex.byName.clear();
    target->objectIndex.byType.clear();
    target->objectIndex.byLayer.clear();

    for (auto &obj : target->Objects)
    {
        if (!obj.name.empty())
            target->objectIndex.byName[obj.name] = &obj;
        if (!obj.type.empty())
            target->objectIndex.byType[obj.type].push_back(&obj);
        if (!obj.layerName.empty())
            target->objectIndex.byLayer[obj.layerName].push_back(&obj);
    }
}

/**
 * @brief Bangun ulang index dari seluruh object pada map aktif
 *
 * Wajib dipanggil setelah LoadMap() atau setiap kali data map berubah.
 */
void BuildMapObjectIndex()
{
    BuildMapObjectIndexTarget(tilesonMap);
    RebuildCollisionCache();
}

/**
 * @brief Bangun ulang cache collision agar IsPositionSafe() gak rebuild vector tiap call
 *
 * Collision data di-cache dari COLLISION_LAYER_NAME.
 * Wajib dipanggil setelah setiap perubahan object index.
 */
void RebuildCollisionCache()
{
    gCollisionCache.rects.clear();
    gCollisionCache.polygons.clear();

    if (!tilesonMap)
        return;

    TiledHelperFunction.TryGetCollisionByLayerName(COLLISION_LAYER_NAME, gCollisionCache);
}

/*==============================================================================
 * Low-level Query Functions
 *==============================================================================*/

/** @brief Cari object berdasarkan nama di target map */
MapObject *TilesonGetObjectByName(TilesonMapData *target, const std::string &name)
{
    auto it = target->objectIndex.byName.find(name);
    return (it != target->objectIndex.byName.end()) ? it->second : nullptr;
}

/** @brief Ambil object berdasarkan type di target map */
const std::vector<MapObject *> &TilesonGetObjectsByType(TilesonMapData *target, const std::string &type)
{
    static const std::vector<MapObject *> empty;
    auto it = target->objectIndex.byType.find(type);
    return (it != target->objectIndex.byType.end()) ? it->second : empty;
}

/** @brief Ambil object berdasarkan layer name di target map */
const std::vector<MapObject *> &TilesonGetObjectsByLayerName(TilesonMapData *target, const std::string &layerName)
{
    static const std::vector<MapObject *> empty;
    auto it = target->objectIndex.byLayer.find(layerName);
    return (it != target->objectIndex.byLayer.end()) ? it->second : empty;
}

/**
 * @brief Cari satu object berdasarkan nama
 * @param name Nama object di Tiled
 * @return Pointer ke object jika ditemukan, nullptr jika tidak ada
 */
MapObject *TilesonGetObjectByName(const std::string &name)
{
    return TilesonGetObjectByName(tilesonMap, name);
}

/**
 * @brief Ambil semua object dengan type tertentu
 * @param type Nilai field Type di Tiled
 * @return Reference ke vector internal, atau vector kosong jika tidak ditemukan
 */
const std::vector<MapObject *> &TilesonGetObjectsByType(const std::string &type)
{
    return TilesonGetObjectsByType(tilesonMap, type);
}

/**
 * @brief Ambil semua object dari layer tertentu
 * @param layerName Nama layer di Tiled
 * @return Reference ke vector internal, atau vector kosong jika tidak ditemukan
 */
const std::vector<MapObject *> &TilesonGetObjectsByLayerName(const std::string &layerName)
{
    return TilesonGetObjectsByLayerName(tilesonMap, layerName);
}

/*==============================================================================
 * TiledHelper
 *==============================================================================*/

/**
 * @brief Ambil posisi object berdasarkan nama
 *
 * Posisi yang dipakai adalah titik kiri atas bounds object.
 *
 * @param objectName Nama object di Tiled
 * @param outPosition [OUT] Posisi object jika ditemukan
 * @return true jika object ditemukan
 */
bool TiledHelper::TryGetObjectPositionByName(const std::string &objectName, Vector2 &outPosition)
{
    MapObject *obj = TilesonGetObjectByName(objectName);
    if (!obj)
        return false;
    outPosition = {obj->bounds.x, obj->bounds.y};
    return true;
}

/**
 * @brief Ambil seluruh collision data dari satu layer
 *
 * Object polygon dimasukkan ke polygons, sisanya ke rects.
 *
 * @param layerName Nama layer di Tiled
 * @param outCollision [OUT] Hasil collision yang ditemukan
 * @return true jika layer berisi object collision
 */
bool TiledHelper::TryGetCollisionByLayerName(const std::string &layerName, CollisionResult &outCollision)
{
    const auto &objs = TilesonGetObjectsByLayerName(layerName);
    if (objs.empty())
        return false;
    for (auto *obj : objs)
    {
        if (obj->hasPolygon)
            outCollision.polygons.push_back(obj->polygonPoints);
        else
            outCollision.rects.push_back(obj->bounds);
    }
    return true;
}

/**
 * @brief Ambil salinan vector pointer object berdasarkan type
 *
 * Dipakai saat caller butuh vector yang aman disimpan sendiri.
 *
 * @param objectType Type object di Tiled
 * @return Copy vector pointer ke object yang sesuai
 */
std::vector<MapObject *> TiledHelper::GetObjectsByType(const std::string &objectType)
{
    const auto &objs = TilesonGetObjectsByType(objectType);
    return std::vector<MapObject *>(objs.begin(), objs.end());
}

/*==============================================================================
 * Hitbox Helpers
 *==============================================================================*/

/**
 * @brief Bangun hitbox rectangle dari posisi dasar dan offset
 *
 * @param position Posisi dasar entity
 * @param offsetX Offset X dari posisi
 * @param offsetY Offset Y dari posisi
 * @param width Lebar hitbox
 * @param height Tinggi hitbox
 * @return Rectangle hitbox yang sudah dihitung
 */
Rectangle BuildHitbox(Vector2 position, float offsetX, float offsetY, float width, float height)
{
    Rectangle hitbox;
    hitbox.x = position.x + offsetX;
    hitbox.y = position.y + offsetY;
    hitbox.width = width;
    hitbox.height = height;
    return hitbox;
}

/**
 * @brief Ambil empat titik sudut dari rectangle
 *
 * Urutan titik:
 * [0] kiri atas, [1] kanan atas, [2] kiri bawah, [3] kanan bawah
 *
 * @param rect Rectangle sumber
 * @return Array berisi empat titik sudut
 */
std::array<Vector2, 4> GetRectangleCorners(const Rectangle &rect)
{
    std::array<Vector2, 4> corners;
    corners[0] = {rect.x, rect.y};
    corners[1] = {rect.x + rect.width, rect.y};
    corners[2] = {rect.x, rect.y + rect.height};
    corners[3] = {rect.x + rect.width, rect.y + rect.height};
    return corners;
}

/*==============================================================================
 * Point-in-Polygon Helpers
 *==============================================================================*/

/**
 * @brief Cek apakah sebuah titik berada di dalam polygon
 *
 * Menggunakan even-odd rule dengan ray casting algorithm.
 *
 * @param point Titik yang ingin dicek
 * @param polygon Daftar titik polygon berurutan
 * @return true jika titik berada di dalam polygon
 */
bool IsPointInPolygon(Vector2 point, const std::vector<Vector2> &polygon)
{
    bool inside = false;
    int pointCount = (int)polygon.size();

    if (pointCount < 3)
        return false; // polygon minimal butuh 3 titik

    // Ray casting algorithm
    for (int i = 0, j = pointCount - 1; i < pointCount; j = i++)
    {
        const Vector2 &pi = polygon[i];
        const Vector2 &pj = polygon[j];

        // Cek apakah ray dari titik ke kanan memotong edge polygon
        bool intersect = ((pi.y > point.y) != (pj.y > point.y)) &&
                         (point.x < (pj.x - pi.x) * (point.y - pi.y) / ((pj.y - pi.y) + 0.00001f) + pi.x);

        if (intersect)
            inside = !inside;
    }

    return inside;
}

/*==============================================================================
 * RayCast
 *==============================================================================*/

/**
 * @brief Cast satu ray dan cari object terdekat yang terkena
 *
 * Ray akan dicek ke seluruh object yang diberikan, lalu dipilih hit terdekat.
 *
 * @param origin Titik asal ray
 * @param direction Arah ray yang sudah dinormalisasi
 * @param maxDistance Jarak maksimum ray
 * @param objects Daftar object yang akan dicek
 * @return Hasil hit terdekat, atau hit=false jika tidak ada tumbukan
 */
RayHitResult RayCast::Cast(Vector2 origin, Vector2 direction, float maxDistance, std::vector<MapObject> &objects)
{
    RayHitResult result = {false, {0, 0}, maxDistance, nullptr};

    for (auto &obj : objects)
    {
        std::optional<float> t;

        if (obj.hasPolygon)
            t = HitPolygon(origin, direction, obj.polygonPoints, maxDistance);
        else
            t = HitRect(origin, direction, obj.bounds, maxDistance);

        if (t && *t < result.distance)
        {
            result.hit = true;
            result.distance = *t;
            result.point = {origin.x + direction.x * *t, origin.y + direction.y * *t};
            result.object = &obj;
        }
    }
    return result;
}

/** @brief Cast cone ray dan return hit terdekat */
RayHitResult RayCast::CastCone(Vector2 origin, Vector2 forward, float maxDistance,
                               float halfAngleDeg, int rayCount,
                               std::vector<MapObject> &objects)
{
    RayHitResult closest = {false, {0, 0}, maxDistance, nullptr};

    if (Vector2Length(forward) < 0.01f)
        return closest;

    float baseAngle = atan2f(forward.y, forward.x);
    float halfRad = halfAngleDeg * DEG2RAD;

    for (int i = 0; i < rayCount; i++)
    {
        float t = (rayCount == 1) ? 0.0f : (float)i / (rayCount - 1);
        float angle = baseAngle + Lerp(-halfRad, halfRad, t);
        Vector2 dir = {cosf(angle), sinf(angle)};

        RayHitResult hit = Cast(origin, dir, maxDistance, objects);
        if (hit.hit && hit.distance < closest.distance)
            closest = hit;
    }

    return closest;
}

/**
 * @brief Cek ray terhadap rectangle dengan memeriksa seluruh sisinya
 *
 * Rectangle dipecah menjadi empat edge lalu tiap edge diuji terhadap ray.
 *
 * @param origin Titik asal ray
 * @param direction Arah ray
 * @param rect Rectangle target
 * @param maxDistance Jarak maksimum ray
 * @return Jarak hit terdekat, atau nullopt jika tidak ada intersection
 */
std::optional<float> RayCast::HitRect(Vector2 origin, Vector2 direction, Rectangle rect, float maxDistance)
{
    auto corners = GetRectangleCorners(rect);
    Vector2 rayEnd = {origin.x + direction.x * maxDistance, origin.y + direction.y * maxDistance};

    std::pair<int, int> edgeIdx[4] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}};

    std::optional<float> closest;
    for (auto &[a, b] : edgeIdx)
    {
        auto t = LineIntersect(origin, rayEnd, corners[a], corners[b]);
        if (t && (!closest || *t < *closest))
            closest = t;
    }
    return closest;
}

/**
 * @brief Cek ray terhadap polygon dengan memeriksa seluruh edge
 *
 * Jika origin sudah berada di dalam polygon, hasilnya langsung 0.
 *
 * @param origin Titik asal ray
 * @param direction Arah ray
 * @param polygon Titik-titik polygon
 * @param maxDistance Jarak maksimum ray
 * @return Jarak hit terdekat, atau nullopt jika tidak ada intersection
 */
std::optional<float> RayCast::HitPolygon(Vector2 origin, Vector2 direction, std::vector<Vector2> &polygon, float maxDistance)
{
    if (polygon.size() < 3)
        return std::nullopt;

    if (IsPointInPolygon(origin, polygon))
        return 0.0f;

    Vector2 rayEnd = {origin.x + direction.x * maxDistance, origin.y + direction.y * maxDistance};
    std::optional<float> closest;

    for (size_t i = 0; i < polygon.size(); i++)
    {
        Vector2 a = polygon[i];
        Vector2 b = polygon[(i + 1) % polygon.size()];
        auto t = LineIntersect(origin, rayEnd, a, b);
        if (t && (!closest || *t < *closest))
            closest = t;
    }
    return closest;
}

/**
 * @brief Hitung intersection antara dua line segment
 *
 * @param p1 Titik awal segment pertama
 * @param p2 Titik akhir segment pertama
 * @param p3 Titik awal segment kedua
 * @param p4 Titik akhir segment kedua
 * @return Jarak dari p1 ke titik potong, atau nullopt jika tidak berpotongan
 */
std::optional<float> RayCast::LineIntersect(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4)
{
    float d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    float d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    float denom = d1x * d2y - d1y * d2x;

    if (fabsf(denom) < 1e-6f)
        return std::nullopt;

    float t = ((p3.x - p1.x) * d2y - (p3.y - p1.y) * d2x) / denom;
    float u = ((p3.x - p1.x) * d1y - (p3.y - p1.y) * d1x) / denom;

    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)
        return t * sqrtf(d1x * d1x + d1y * d1y);

    return std::nullopt;
}

/*==============================================================================
 * Collision Check Helpers
 *==============================================================================*/

/**
 * @brief Cek collision hitbox terhadap kumpulan rectangle
 *
 * @param hitbox Hitbox entity yang diperiksa
 * @param collisionRects Daftar rectangle collision
 * @return true jika ada tabrakan
 */
bool CheckCollisionAgainstRects(const Rectangle &hitbox, const std::vector<Rectangle> &collisionRects)
{
    for (const auto &rect : collisionRects)
    {
        if (CheckCollisionRecs(hitbox, rect))
            return true;
    }
    return false;
}

/**
 * @brief Cek collision hitbox terhadap kumpulan polygon
 *
 * Collision dianggap terjadi jika salah satu sudut hitbox berada di dalam polygon.
 *
 * @param hitbox Hitbox entity yang diperiksa
 * @param collisionPolygons Daftar polygon collision
 * @return true jika ada tabrakan
 */
bool CheckCollisionAgainstPolygons(const Rectangle &hitbox, const std::vector<std::vector<Vector2>> &collisionPolygons)
{
    std::array<Vector2, 4> corners = GetRectangleCorners(hitbox);

    for (const auto &polygon : collisionPolygons)
    {
        for (int i = 0; i < 4; i++)
        {
            if (IsPointInPolygon(corners[i], polygon))
                return true;
        }
    }
    return false;
}

/**
 * @brief Cek apakah hitbox masih berada di dalam batas dunia
 *
 * @param hitbox Hitbox entity yang diperiksa
 * @param worldWidth Lebar dunia dalam pixel
 * @param worldHeight Tinggi dunia dalam pixel
 * @return true jika hitbox masih berada di dalam batas dunia
 */
bool IsWithinWorldBounds(const Rectangle &hitbox, float worldWidth, float worldHeight)
{
    // Cek batas kiri dan atas
    if (hitbox.x < 0.0f)
        return false;
    if (hitbox.y < 0.0f)
        return false;

    // Cek batas kanan dan bawah
    if (hitbox.x + hitbox.width > worldWidth)
        return false;
    if (hitbox.y + hitbox.height > worldHeight)
        return false;

    return true;
}

/**
 * @brief Cek apakah sebuah posisi aman (tidak menabrak wall/collision) dan berada di dalam batas map
 *
 * Menggabungkan pengecekan world bounds dan collision terhadap rectangle & polygon
 * yang ada di layer COLLISION_LAYER_NAME.
 *
 * @param pos Posisi entitas yang dicek
 * @param width Lebar hitbox
 * @param height Tinggi hitbox
 * @param offsetX Offset X hitbox dari posisi dasar
 * @param offsetY Offset Y hitbox dari posisi dasar
 * @return true jika posisi aman (di dalam map dan tidak menabrak)
 */
bool IsPositionSafe(Vector2 pos, float width, float height, float offsetX, float offsetY)
{
    if (!tilesonMap)
        return true;

    // 1. Bangun hitbox
    Rectangle hitbox = BuildHitbox(pos, offsetX, offsetY, width, height);

    // 2. Cek World Bounds
    float worldWidth = (float)tilesonMap->width * FRAME_SIZE;
    float worldHeight = (float)tilesonMap->height * FRAME_SIZE;
    if (!IsWithinWorldBounds(hitbox, worldWidth, worldHeight))
        return false;

    // 3. Cek Collision via cache (Obstacle Layer)
    // Pakai gCollisionCache biar gak rebuild vector tiap call
    if (CheckCollisionAgainstRects(hitbox, gCollisionCache.rects))
        return false;
    if (CheckCollisionAgainstPolygons(hitbox, gCollisionCache.polygons))
        return false;

    // 4. Cek Dynamic Obstacles (object runtime seperti bomb)
    if (CheckCollisionAgainstRects(hitbox, DynamicObstacles))
        return false;

    return true;
}

// dynamic obstacle function
std::vector<Rectangle> DynamicObstacles;
