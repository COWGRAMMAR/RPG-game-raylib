#pragma once

#include "../lib/raylib/include/raylib.h"
#include "buttonTxt.h"

/** @brief Generic popup UI */
class Popup 
{
public:
    /** @brief Constructor default */
    Popup();
    /** @brief Constructor dengan pesan dan tombol */
    Popup(const char* message, const char* buttonText, float hoverAmount = 1.0F);
    /** @brief Destructor */
    ~Popup();

    /** @brief Tampilkan popup */
    void Show();
    /** @brief Sembunyikan popup */
    void Hide();
    /** @brief Cek apakah popup aktif */
    bool IsActive() const;

    /** @brief Update state popup */
    void Update(Vector2 mousePosition, bool mouseClicked);
    /** @brief Render popup ke layar */
    void Draw(Vector2 mousePosition);

private:
    /** @brief Hitung dimensi popup berdasarkan teks */
    void CalculateDimensions();

    bool active;
    const char* message;
    const char* buttonText;
    buttonTxt okButton;
    float hoverAmount;

    Vector2 position;
    int width;
    int height;
    Rectangle backgroundRect;
};
