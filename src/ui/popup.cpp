#include <algorithm>

#include "ui/popup.h"
#include "core/screen.h"
#include "rendering/fonts.h"

/**
 * @brief Default constructor.
 */
// konstruktor default dengan inisialisasi textYOffset, buttonYOffset, bgTexture, textColor
Popup::Popup() : active(false), hasCancelButton(false), message(nullptr), subMessage(nullptr),
                 buttonText(nullptr), cancelText(nullptr), hoverAmount(1.0F), confirmClicked(false), position({0, 0}), width(0), height(0), bgTexture({0}), textYOffset(0), buttonYOffset(0), textColor(WHITE), buttonTrim(0)
{
}

/**
 * @brief Parameterized constructor.
 * @param message Teks yang ditampilkan di popup.
 * @param buttonText Teks pada tombol.
 * @param hoverAmount Nilai pengurangan warna saat hover (0.0 = hitam, 1.0 = normal).
 */
// konstruktor 1 tombol dengan inisialisasi textYOffset, buttonYOffset, bgTexture, textColor
Popup::Popup(const char *message, const char *buttonText, float hoverAmount)
    : active(false), hasCancelButton(false), message(message), subMessage(nullptr),
      buttonText(buttonText), cancelText(nullptr), hoverAmount(hoverAmount), confirmClicked(false), position({0, 0}), width(0), height(0), bgTexture({0}), textYOffset(0), buttonYOffset(0), textColor(WHITE), buttonTrim(0)
{
}

/**
 * @brief Two-button constructor.
 * @param message Teks yang ditampilkan di popup.
 * @param confirmText Teks pada tombol konfirmasi.
 * @param cancelText Teks pada tombol batal.
 * @param hoverAmount Nilai pengurangan warna saat hover (0.0 = hitam, 1.0 = normal).
 */
// konstruktor 2 tombol dengan inisialisasi textYOffset, buttonYOffset, bgTexture, textColor
Popup::Popup(const char *message, const char *confirmText, const char *cancelText, float hoverAmount)
    : active(false), hasCancelButton(true), message(message), subMessage(nullptr),
      buttonText(confirmText), cancelText(cancelText), hoverAmount(hoverAmount), confirmClicked(false),
      position({0, 0}), width(0), height(0), bgTexture({0}), textYOffset(0), buttonYOffset(0), textColor(WHITE), buttonTrim(0)
{
}

/**
 * @brief Destructor.
 */
// unload bgTexture di destructor
Popup::~Popup()
{
    if (bgTexture.id != 0)
    {
        UnloadTexture(bgTexture);
    }
}

// method baru untuk mengatur background texture popup
void Popup::SetBackgroundTexture(const char *path)
{
    if (bgTexture.id != 0)
    {
        UnloadTexture(bgTexture);
    }
    Image img = LoadImage(path);
    if (img.data != nullptr)
    {
        bgTexture = LoadTextureFromImage(img);
        UnloadImage(img);
    }
}

// method untuk mengatur offset Y teks dan tombol
void Popup::SetTextYOffset(int offset) { textYOffset = offset; }
void Popup::SetButtonYOffset(int offset) { buttonYOffset = offset; }

void Popup::SetSubMessage(const char *sub)
{
    subMessage = sub;
}

// basch-3: set warna teks pesan & tombol
void Popup::SetTextColor(Color color)
{
    textColor = color;
}

// basch-3: trim piksel dari lebar tombol agar hitbox sesuai visual
void Popup::SetButtonTrim(int trim)
{
    buttonTrim = trim;
}

/**
 * @brief Show()
 * Tampilkan popup ke layar.
 */
void Popup::Show()
{
    confirmClicked = false;
    CalculateDimensions();
    active = true;
}

/**
 * @brief Hide()
 * Sembunyikan popup dari layar.
 */
void Popup::Hide()
{
    active = false;
    // NOTE: do NOT reset confirmClicked here - Update() sets it before calling Hide(),
    // and the main menu needs to read it via IsConfirmClicked() after the popup closes.
    // confirmClicked is reset in Show() and the constructor.
}

/**
 * @brief IsActive()
 * @return true jika popup sedang aktif.
 */
bool Popup::IsActive() const
{
    return active;
}

/**
 * @brief IsConfirmClicked()
 * @return true jika tombol konfirmasi telah diklik sejak Show() terakhir.
 */
bool Popup::IsConfirmClicked() const
{
    return confirmClicked;
}

/**
 * @brief Update()
 * Update state popup - handle klik tombol OK.
 * @param mousePosition Posisi mouse saat ini.
 * @param mouseClicked true jika tombol mouse diklik.
 */
void Popup::Update(Vector2 mousePosition, bool mouseClicked)
{
    if (!active)
    {
        return;
    }

    if (okButton.isClicked(mousePosition, mouseClicked))
    {
        if (hasCancelButton)
        {
            confirmClicked = true;
        }
        Hide();
        return;
    }

    if (hasCancelButton && cancelButton.isClicked(mousePosition, mouseClicked))
    {
        Hide();
    }
}

/**
 * @brief CalculateDimensions()
 * Hitung dimensi popup berdasarkan teks message dan button.
 */
// CalculateDimensions pakai GetOrLoad(FontId::LOADING_TITLE) untuk ukur teks & buat tombol
void Popup::CalculateDimensions()
{
    const int paddingX = 40;
    const int paddingY = 30;
    const int textPadding = 20;
    const int maxWidth = static_cast<int>(GameScreenWidth * 0.6F);
    const int fontSize = 30;
    const int subMessageSpacing = 10;

    Vector2 msgSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), message, fontSize, 0);
    int textWidth = static_cast<int>(msgSize.x);
    int subWidth = (subMessage != nullptr) ? static_cast<int>(MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), subMessage, fontSize, 0).x) : 0;
    Vector2 btnSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), buttonText, fontSize, 0);
    int buttonWidth = static_cast<int>(btnSize.x);

    // Extra height for sub-message line
    int subExtraHeight = (subMessage != nullptr) ? (fontSize + subMessageSpacing) : 0;

    if (hasCancelButton)
    {
        Vector2 cancelSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), cancelText, fontSize, 0);
        int cancelWidth = static_cast<int>(cancelSize.x);
        int effBtnWidth = buttonWidth - buttonTrim;   // basch-3: trim lebar tombol
        int effCancelWidth = cancelWidth - buttonTrim; // basch-3: trim lebar cancel
        int buttonsTotalWidth = effBtnWidth + 25 + effCancelWidth; // basch-3: gap 20→25

        int contentWidth = std::max({textWidth, subWidth, buttonsTotalWidth});
        width = contentWidth + (paddingX * 2);
        width = std::min(width, maxWidth);

        height = fontSize + (fontSize * 2) + (paddingY * 3) + textPadding + subExtraHeight;

        position.x = (GameScreenWidth - width) / 2.0F;
        position.y = (GameScreenHeight - height) / 2.0F;

        backgroundRect = {position.x, position.y, static_cast<float>(width), static_cast<float>(height)};

        int startX = static_cast<int>(position.x + ((width - buttonsTotalWidth) / 2.0F));
        // posisi Y tombol pakai default -5 + buttonYOffset
        int buttonY = static_cast<int>(position.y + height - paddingY - fontSize) - 5 + buttonYOffset;

        // tombol pakai textColor dan GetOrLoad(FontId::LOADING_TITLE)
        okButton = buttonTxt(buttonText, startX, buttonY, fontSize, textColor, hoverAmount, GetOrLoad(FontId::LOADING_TITLE));
        cancelButton = buttonTxt(cancelText, startX + effBtnWidth + 25, buttonY, fontSize, textColor, hoverAmount, GetOrLoad(FontId::LOADING_TITLE)); // basch-3: gap 25
    }
    else
    {
        int effBtnWidth = buttonWidth - buttonTrim; // basch-3: trim lebar tombol
        int contentWidth = std::max({textWidth, subWidth, effBtnWidth});
        width = contentWidth + (paddingX * 2);
        width = std::min(width, maxWidth);

        height = fontSize + (fontSize * 2) + (paddingY * 3) + textPadding + subExtraHeight;

        position.x = (GameScreenWidth - width) / 2.0F;
        position.y = (GameScreenHeight - height) / 2.0F;

        backgroundRect = {position.x, position.y, static_cast<float>(width), static_cast<float>(height)};

        int buttonX = static_cast<int>(position.x + ((width - effBtnWidth) / 2.0F));
        // posisi Y tombol pakai default -5 + buttonYOffset
        int buttonY = static_cast<int>(position.y + height - paddingY - fontSize) - 5 + buttonYOffset;

        TraceLog(LOG_DEBUG, "Popup Debug: width=%d, height=%d", width, height);
        TraceLog(LOG_DEBUG, "Popup Debug: position=(%.1f, %.1f)", position.x, position.y);
        TraceLog(LOG_DEBUG, "Popup Debug: buttonX=%d, buttonY=%d, buttonWidth=%d", buttonX, buttonY, effBtnWidth);

        // tombol pakai textColor dan GetOrLoad(FontId::LOADING_TITLE)
        okButton = buttonTxt(buttonText, buttonX, buttonY, fontSize, textColor, hoverAmount, GetOrLoad(FontId::LOADING_TITLE));
    }
}

/**
 * @brief Draw()
 * Render popup: overlay, background, message, dan button.
 * @param mousePosition Posisi mouse saat ini.
 */
void Popup::Draw(Vector2 mousePosition)
{
    if (!active)
    {
        return;
    }

    Color overlayColor = {
        0,
        0,
        0,
        static_cast<unsigned char>(255 * 0.5F)};

    Rectangle fullScreen = {0, 0, static_cast<float>(GameScreenWidth), static_cast<float>(GameScreenHeight)};
    DrawRectangleRec(fullScreen, overlayColor);

    // bgTexture di-center di area popup jika ada
    if (bgTexture.id != 0)
    {
        int texX = static_cast<int>(position.x + (width - bgTexture.width) / 2.0F);
        int texY = static_cast<int>(position.y + (height - bgTexture.height) / 2.0F);
        DrawTexture(bgTexture, texX, texY, WHITE);
    }
    else
    {
        Color bgColor = {20, 20, 20, 255};
        DrawRectangleRec(backgroundRect, bgColor);
        DrawRectangleLines(
            static_cast<int>(backgroundRect.x),
            static_cast<int>(backgroundRect.y),
            static_cast<int>(backgroundRect.width),
            static_cast<int>(backgroundRect.height),
            WHITE);
    }

    int fontSize = 30;
    // teks pesan pakai GetOrLoad(FontId::LOADING_TITLE), WHITE, dengan offset Y +20 + textYOffset
    Vector2 textSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), message, fontSize, 0);
    int textX = static_cast<int>(position.x + ((width - textSize.x) / 2.0F));
    int textY = static_cast<int>(position.y + (hasCancelButton ? 20 : 30) + 20 + textYOffset);

    DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), message, Vector2{static_cast<float>(textX), static_cast<float>(textY)}, fontSize, 0, textColor);

    // subMessage pakai GetOrLoad(FontId::LOADING_TITLE), WHITE, posisi Y original
    if (subMessage != nullptr)
    {
        Vector2 subSize = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), subMessage, fontSize, 0);
        int subX = static_cast<int>(position.x + ((width - subSize.x) / 2.0F));
        int subY = textY + fontSize + 10;
        DrawTextEx(GetOrLoad(FontId::LOADING_TITLE), subMessage, Vector2{static_cast<float>(subX), static_cast<float>(subY)}, fontSize, 0, textColor);
    }

    // highlight background saat hover — ukur teks pake GetOrLoad(FontId::LOADING_TITLE) biar pas
    if (okButton.isHovered(mousePosition))
    {
        Rectangle b = okButton.GetBounds();
        Vector2 ts = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), buttonText, fontSize, 0);
        DrawRectangleRounded((Rectangle){b.x, b.y, ts.x, ts.y}, 0.3f, 8, ColorAlpha(WHITE, 0.25f));
    }
    if (hasCancelButton && cancelButton.isHovered(mousePosition))
    {
        Rectangle b = cancelButton.GetBounds();
        Vector2 ts = MeasureTextEx(GetOrLoad(FontId::LOADING_TITLE), cancelText, fontSize, 0);
        DrawRectangleRounded((Rectangle){b.x, b.y, ts.x, ts.y}, 0.3f, 8, ColorAlpha(WHITE, 0.25f));
    }
    // tombol pakai WHITE via buttonTxt (di-set di CalculateDimensions)
    okButton.Draw(mousePosition);
    if (hasCancelButton)
    {
        cancelButton.Draw(mousePosition);
    }
}
