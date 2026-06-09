#pragma once

#include "../lib/raylib/include/raylib.h"
#include "../lib/json/include/nlohmann/json.hpp"

/*==============================================================================
 * Parsing Helpers — konversi tipe nlohmann::json ke tipe Raylib/custom
 *==============================================================================*/

namespace DataDriven
{
    /** @brief Parse {"x": float, "y": float} → Vector2 */
    Vector2 ParseVector2(const nlohmann::json& j);

    /** @brief Parse {"x": float, "y": float, "width": float, "height": float} → Rectangle */
    Rectangle ParseRectangle(const nlohmann::json& j);

    /** @brief Safe get dengan fallback jika field tidak ada */
    template<typename T>
    T SafeGet(const nlohmann::json& j, const std::string& key, const T& fallback)
    {
        if (j.contains(key)) return j.at(key).get<T>();
        return fallback;
    }
}