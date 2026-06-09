#include "datadriven.h"

namespace DataDriven
{
    Vector2 ParseVector2(const nlohmann::json &j)
    {
        return Vector2{
            j.at("x").get<float>(),
            j.at("y").get<float>()};
    }

    Rectangle ParseRectangle(const nlohmann::json &j)
    {
        return Rectangle{
            j.at("x").get<float>(),
            j.at("y").get<float>(),
            j.at("width").get<float>(),
            j.at("height").get<float>()};
    }
}