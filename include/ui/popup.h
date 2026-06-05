#pragma once

#include "../../lib/raylib/include/raylib.h"
#include "buttonTxt.h"

/** @brief Generic popup UI */
class Popup 
{
public:
    /** @brief Constructor default */
    Popup();
    /** @brief Constructor dengan pesan dan tombol */
    Popup(const char* message, const char* buttonText, float hoverAmount = 1.0F);
    Popup(const char* message, const char* confirmText, const char* cancelText, float hoverAmount);
    ~Popup();

    /** @brief Tampilkan popup */
    void Show();
    /** @brief Sembunyikan popup */
    void Hide();
    /** @brief Cek apakah popup aktif */
    bool IsActive() const;
    bool IsConfirmClicked() const;

    void SetSubMessage(const char* sub);

    /** @brief Update state popup */
    void Update(Vector2 mousePosition, bool mouseClicked);
    /** @brief Render popup ke layar */
    void Draw(Vector2 mousePosition);

private:
    /** @brief Hitung dimensi popup berdasarkan teks */
    void CalculateDimensions();

    bool active;
    bool hasCancelButton;
    const char* message;
    const char* subMessage;
    const char* buttonText;
    const char* cancelText;
    buttonTxt okButton;
    buttonTxt cancelButton;
    float hoverAmount;
    bool confirmClicked;

    Vector2 position;
    int width;
    int height;
    Rectangle backgroundRect;
};
