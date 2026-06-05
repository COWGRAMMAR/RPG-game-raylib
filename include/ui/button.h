#pragma once

/**
 * @file button.h
 * @brief Generic Button Template System
 *
 * Template-based button system yang support berbagai tipe content.
 *Policy-based design buat flexibility.
 */

#include "../lib/raylib/include/raylib.h"
#include <string>
#include <iostream>
#include <type_traits>

/*==============================================================================
 * Button Policies
 *==============================================================================*/

/**
 * @brief Policy struct untuk text-based button
 * @note hold data dan behavior untuk tombol berbasis teks
 */
struct TextPolicy
{
    const char *text;
    int posX;
    int posY;
    int fontSize;
    Color textColor;
    int textWidth;
    float hoverAmount;

    /** @brief Default constructor */
    TextPolicy() : text(nullptr), posX(0), posY(0), fontSize(0), textColor(BLANK), textWidth(0), hoverAmount(1.0F) {}

    /** @brief Construct dengan text, posisi, font, warna, hover */
    TextPolicy(const char *text, int posX, int posY, int fontSize, Color color, float hover)
        : text(text), posX(posX), posY(posY), fontSize(fontSize), textColor(color), hoverAmount(hover)
    {
        textWidth = MeasureText(text, fontSize);
    }

    /** @brief Dapatkan bounds button text */
    [[nodiscard]] Rectangle GetBounds() const
    {
        return {static_cast<float>(posX), static_cast<float>(posY), 
                static_cast<float>(textWidth), static_cast<float>(fontSize)};
    }
};

/**
 * @brief Policy struct untuk image-based button
 * @note hold data dan behavior untuk tombol berbasis texture
 */
struct ImagePolicy
{
    Texture2D texture;
    Vector2 position;
    float hoverAmount;

    /** @brief Default constructor */
    ImagePolicy() : texture({0}), position({0, 0}), hoverAmount(1.0F) {}

    /** @brief Construct dari file texture dengan scale dan hover */
    ImagePolicy(const char *texturePath, Vector2 pos, float scale, float hover)
    {
        Image image = LoadImage(texturePath);
        int newWidth = static_cast<int>(image.width * scale);
        int newHeight = static_cast<int>(image.height * scale);
        ImageResize(&image, newWidth, newHeight);
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
        position = pos;
        hoverAmount = hover;
    }

    ImagePolicy(const ImagePolicy&) = delete;
    ImagePolicy& operator=(const ImagePolicy&) = delete;

    ImagePolicy(ImagePolicy&& other) noexcept
        : texture(other.texture), position(other.position), hoverAmount(other.hoverAmount)
    {
        other.texture = {0};
    }

    ImagePolicy& operator=(ImagePolicy&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            texture = other.texture;
            position = other.position;
            hoverAmount = other.hoverAmount;
            other.texture = {0};
        }
        return *this;
    }

    ~ImagePolicy()
    {
        Unload();
    }

    void Unload()
    {
        if (texture.id != 0)
        {
            UnloadTexture(texture);
            texture = {0};
        }
    }

    /** @brief Dapatkan bounds button image */
    [[nodiscard]] Rectangle GetBounds() const
    {
        float halfW = static_cast<float>(texture.width) / 2.0F;
        float halfH = static_cast<float>(texture.height) / 2.0F;
        return {position.x - halfW, position.y - halfH,
                static_cast<float>(texture.width), static_cast<float>(texture.height)};
    }
};

/*==============================================================================
 * Button Template Class
 *==============================================================================*/

/**
 * @brief Generic Button template class
 * @tparam PolicyType TextPolicy atau ImagePolicy
 *
 * Template class yang bisa handle berbagai tipe button content.
 * Gunakan policy buat define cara render dan collision detection.
 */
template<typename PolicyType>
class Button
{
public:
    /** @brief Default constructor */
    Button() : policy() {}

    /** @brief Construct button dengan args untuk policy */
    template<typename... Args>
    Button(Args... args) : policy(args...)
    {
    }

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;
    Button(Button&&) = default;
    Button& operator=(Button&&) = default;

    /** @brief Destructor (unload texture kalo ImagePolicy) */
    ~Button()
    {
        if constexpr (std::is_same_v<PolicyType, ImagePolicy>)
        {
            policy.Unload();
        }
    }

    /** @brief Render button */
    void Draw(Vector2 mousePosition)
    {
        Color currentColor = GetNormalColor();
        if (isHovered(mousePosition))
        {
            currentColor = DarkenColor(currentColor, policy.hoverAmount);
        }
        Render(currentColor);
    }

    /** @brief Cek klik */
    [[nodiscard]] bool isClicked(Vector2 mousePosition, bool mouseClicked) const
    {
        return CheckCollisionPointRec(mousePosition, policy.GetBounds()) && mouseClicked;
    }

    /** @brief Cek hover */
    [[nodiscard]] bool isHovered(Vector2 mousePosition) const
    {
        return CheckCollisionPointRec(mousePosition, policy.GetBounds());
    }

    /** @brief Dapatkan bounds */
    [[nodiscard]] Rectangle GetBounds() const
    {
        return policy.GetBounds();
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator==(const Button &other) const
    {
        Rectangle a = policy.GetBounds();
        Rectangle b = other.policy.GetBounds();
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator!=(const Button &other) const
    {
        return !(*this == other);
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator<(const Button &other) const
    {
        Rectangle a = policy.GetBounds();
        Rectangle b = other.policy.GetBounds();
        if (a.y != b.y)
            return a.y < b.y;
        return a.x < b.x;
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator<=(const Button &other) const
    {
        return !(other < *this);
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator>(const Button &other) const
    {
        return other < *this;
    }

    /** @brief Perbandingan bounds */
    [[nodiscard]] bool operator>=(const Button &other) const
    {
        return !(*this < other);
    }

private:
    /** @brief Dapatkan warna normal sesuai policy */
    [[nodiscard]] Color GetNormalColor() const
    {
        if constexpr (std::is_same_v<PolicyType, TextPolicy>)
        {
            return policy.textColor;
        }
        else
        {
            return WHITE;
        }
    }

    /** @brief Render button sesuai policy type */
    void Render(Color color)
    {
        if constexpr (std::is_same_v<PolicyType, TextPolicy>)
        {
            DrawText(policy.text, policy.posX, policy.posY, policy.fontSize, color);
        }
        else
        {
            Vector2 drawPos = {
                policy.position.x - static_cast<float>(policy.texture.width) / 2.0F,
                policy.position.y - static_cast<float>(policy.texture.height) / 2.0F
            };
            DrawTextureV(policy.texture, drawPos, color);
        }
    }

    /** @brief Gelapkan warna dengan faktor amount */
    static Color DarkenColor(Color color, float amount)
    {
        return {
            static_cast<unsigned char>(color.r * amount),
            static_cast<unsigned char>(color.g * amount),
            static_cast<unsigned char>(color.b * amount),
            color.a
        };
    }

    PolicyType policy; // Policy instance
};

/*==============================================================================
 * Type Aliases (Backward Compatibility)
 *==============================================================================*/

/**
 * @brief Text-based button - backward compatible dengan buttonTxt lama
 */
using buttonTxt = Button<TextPolicy>;

/**
 * @brief Image-based button - backward compatible dengan buttonImage lama
 */
using buttonImage = Button<ImagePolicy>;

/*==============================================================================
 * Function Overloading Demonstration
 *==============================================================================*/

template<typename PolicyType>
class Button;

namespace ButtonHelper
{
inline void print(int value)
{
    DrawText(TextFormat("Int: %d", value), 0, 0, 20, WHITE);
}

inline void print(float value)
{
    DrawText(TextFormat("Float: %.2f", value), 0, 0, 20, WHITE);
}

inline void print(double value)
{
    DrawText(TextFormat("Double: %.2f", value), 0, 0, 20, WHITE);
}

inline void print(const char *text)
{
    DrawText(text, 0, 0, 20, WHITE);
}

template<typename PolicyType>
inline void print(const Button<PolicyType> &btn)
{
    Rectangle bounds = btn.GetBounds();
    DrawText(TextFormat("Button at (%.0f, %.0f) size (%.0f x %.0f)",
           bounds.x, bounds.y, bounds.width, bounds.height),
           0, 0, 20, WHITE);
}

template<typename PolicyType>
inline std::ostream &operator<<(std::ostream &os, const Button<PolicyType> &btn)
{
    Rectangle bounds = btn.GetBounds();
    os << "Button<" << (std::is_same_v<PolicyType, TextPolicy> ? "TextPolicy" : "ImagePolicy") << ">{";
    os << "pos=(" << bounds.x << "," << bounds.y << "), "
       << "size=" << bounds.width << "x" << bounds.height << "}";
    return os;
}
} // namespace ButtonHelper