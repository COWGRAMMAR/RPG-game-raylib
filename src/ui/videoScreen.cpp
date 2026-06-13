#include "videoScreen.h"
#include "fonts.h"

/** @name Lifecycle */
/**@{*/

VideoScreen::VideoScreen()
    : m_videoPath("assets/video/dolby-countdown.mkv")
    , m_skipRequested(false)
    , m_loaded(false)
{
}

VideoScreen::~VideoScreen()
{
    Unload();
}

/**@}*/

/** @name Playback */
/**@{*/

bool VideoScreen::LoadAndPlay()
{
    m_skipRequested = false;

    if (m_loaded)
        Unload();

    TraceLog(LOG_INFO, "VIDEO: Loading '%s'...", m_videoPath.c_str());

    m_player.Load(m_videoPath);
    m_loaded = m_player.IsValid();

    if (m_loaded)
    {
        TraceLog(LOG_INFO, "VIDEO: Loaded successfully (%dx%d, %.1fs)",
                 m_player.GetWidth(), m_player.GetHeight(), m_player.GetDuration());
        m_player.Play();
        TraceLog(LOG_INFO, "VIDEO: Playback started");
    }
    else
    {
        TraceLog(LOG_WARNING, "VIDEO: Failed to load '%s' -- skipping to main menu", m_videoPath.c_str());
        m_skipRequested = true;
    }

    return m_player.IsValid();
}

void VideoScreen::Unload()
{
    if (m_loaded)
    {
        TraceLog(LOG_INFO, "VIDEO: Unloading '%s'", m_videoPath.c_str());
    }
    m_player.Unload();
    m_loaded = false;
}

/**@}*/

/** @name Setters / Getters */
/**@{*/

void VideoScreen::SetVideoPath(const std::string &path)
{
    m_videoPath = path;
}

const std::string &VideoScreen::GetVideoPath() const
{
    return m_videoPath;
}

void VideoScreen::SetVolume(float vol)
{
    m_player.SetVolume(vol);
}

// -----------------------------------------------------------------------------
// Update & Render
// -----------------------------------------------------------------------------

bool VideoScreen::Update(float deltaTime)
{
    // Cek input skip -- dilakukan SEBELUM guard !m_loaded agar
    // user tetap bisa skip meskipun video gagal dimuat.
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
    {
        m_skipRequested = true;
    }

    // Jika video belum dimuat, jangan lanjut ke update player
    if (!m_loaded)
    {
        return m_skipRequested;
    }

    m_player.Update(deltaTime);

    // Transisi jika video selesai atau di-skip
    if (m_player.IsFinished())
    {
        TraceLog(LOG_INFO, "VIDEO: Playback finished (%.1fs)", m_player.GetPosition());
        return true;
    }

    if (m_skipRequested)
    {
        TraceLog(LOG_INFO, "VIDEO: Skipped by user at %.1fs", m_player.GetPosition());
        return true;
    }

    return false;
}

void VideoScreen::Draw()
{
    ClearBackground(BLACK);

    if (m_player.IsValid())
    {
        // Hitung posisi dan ukuran agar video fit di window dengan aspek rasio terjaga
        const int videoW = m_player.GetWidth();
        const int videoH = m_player.GetHeight();
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();

        // Hindari division-by-zero bila texture video belum siap
        if (videoW == 0 || videoH == 0)
            return;

        // Skala proporisional
        const float scaleX = static_cast<float>(screenW) / static_cast<float>(videoW);
        const float scaleY = static_cast<float>(screenH) / static_cast<float>(videoH);
        const float scale = (scaleX < scaleY) ? scaleX : scaleY;

        const int drawW = static_cast<int>(static_cast<float>(videoW) * scale);
        const int drawH = static_cast<int>(static_cast<float>(videoH) * scale);
        const int drawX = (screenW - drawW) / 2;
        const int drawY = (screenH - drawH) / 2;

        m_player.Draw(drawX, drawY, drawW, drawH, WHITE);
    }
    else if (!m_loaded)
    {
        // Tampilkan teks "Memuat video..." jika belum dimuat
        const char *loadingText = "Memuat video...";
        const int fontSize = 20;
        const int textW = MeasureText(loadingText, fontSize);
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();

        DrawDefaultText(
            loadingText,
            (screenW - textW) / 2,
            screenH / 2 - fontSize / 2,
            fontSize,
            WHITE);
    }

    // Teks hint skip di pojok kanan bawah
    {
        const char *skipText = "Tekan SPACE untuk skip";
        const int fontSize = 20;
        const int textW = MeasureText(skipText, fontSize);
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        const Color hintColor = {255, 255, 255, 180};

        DrawDefaultText(
            skipText,
            screenW - textW - 20,
            screenH - fontSize - 20,
            fontSize,
            hintColor);
    }
}
