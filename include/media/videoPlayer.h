#pragma once

/**
 * @file videoPlayer.h
 * @brief VideoPlayer class - C++ wrapper around raylib-media's MediaStream C API.
 *
 * Encapsulates video playback state and exposes a clean object-oriented interface
 * for loading, playing, pausing, stopping, and rendering video files.
 */

#include <string>
#include "raylib.h"
#include "raymedia.h"

namespace video
{

    /**
     * @brief RAII-style video player that wraps a raylib-media MediaStream.
     *
     * Usage:
     * @code
     *   video::VideoPlayer player;
     *   if (player.Load("assets/videos/intro.mp4"))
     *   {
     *       player.Play();
     *       while (!player.IsFinished())
     *       {
     *           player.Update(GetFrameTime());
     *           player.Draw(0, 0, 640, 360);
     *       }
     *   }
     * @endcode
     */
    class VideoPlayer
    {
    public:
        // -------------------------------------------------------------------------
        // Construction / Destruction
        // -------------------------------------------------------------------------

        VideoPlayer() = default;
        ~VideoPlayer();

        // -------------------------------------------------------------------------
        // Loading / Unloading
        // -------------------------------------------------------------------------

        bool Load(const std::string &filePath);
        void Unload();

        // -------------------------------------------------------------------------
        // Playback Control
        // -------------------------------------------------------------------------

        void Play();
        void Pause();
        void Stop();

        // -------------------------------------------------------------------------
        // Per-Frame Update and Render
        // -------------------------------------------------------------------------

        void Update(float deltaTime);
        void Draw(int x, int y, int width, int height, Color tint = WHITE);

        // -------------------------------------------------------------------------
        // Configuration
        // -------------------------------------------------------------------------

        void SetLooping(bool loop);
        void SetVolume(float vol);
        bool Seek(double timeSec);

        // -------------------------------------------------------------------------
        // Query
        // -------------------------------------------------------------------------

        double GetPosition() const;
        double GetDuration() const;
        int GetWidth() const;
        int GetHeight() const;

        bool IsFinished() const;
        bool IsValid() const;
        bool IsPlaying() const;
        bool IsPaused() const;

    private:
        MediaStream m_stream{}; ///< Underlying C media stream
        bool m_valid{};         ///< True after successful Load()
        bool m_finished{};      ///< True when non-looping playback reaches the end
        bool m_playing{};       ///< True while the user expects playback
        float m_volume{1.0f};   ///< Volume in [0.0, 1.0]
    };

    // =============================================================================
    // Implementasi Inline
    // =============================================================================

    inline VideoPlayer::~VideoPlayer()
    {
        Unload();
    }

    inline bool VideoPlayer::Load(const std::string &filePath)
    {
        // Bersihkan media yang sudah dimuat sebelumnya
        Unload();

        m_stream = LoadMediaEx(filePath.c_str(), MEDIA_FLAG_NO_AUTOPLAY);
        m_valid = IsMediaValid(m_stream);

        if (m_valid)
        {
            SetAudioStreamVolume(m_stream.audioStream, m_volume);
        }

        return m_valid;
    }

    inline void VideoPlayer::Unload()
    {
        if (m_valid)
        {
            UnloadMedia(&m_stream);
        }

        m_stream = MediaStream{};
        m_valid = false;
        m_finished = false;
        m_playing = false;
    }

    inline void VideoPlayer::Play()
    {
        if (!m_valid)
            return;

        SetMediaState(m_stream, MEDIA_STATE_PLAYING);
        m_playing = true;
        m_finished = false;
    }

    inline void VideoPlayer::Pause()
    {
        if (!m_valid)
            return;

        SetMediaState(m_stream, MEDIA_STATE_PAUSED);
        m_playing = false;
    }

    inline void VideoPlayer::Stop()
    {
        if (!m_valid)
            return;

        SetMediaState(m_stream, MEDIA_STATE_STOPPED);
        m_playing = false;
        m_finished = false;
    }

    inline void VideoPlayer::Update(float deltaTime)
    {
        if (!m_valid)
            return;

        UpdateMediaEx(&m_stream, static_cast<double>(deltaTime));

        // Deteksi akhir pemutaran saat tidak looping
        if (m_playing)
        {
            const int state = GetMediaState(m_stream);
            if (state == MEDIA_STATE_STOPPED)
            {
                m_playing = false;
                m_finished = true;
            }
        }
    }

    inline void VideoPlayer::Draw(int x, int y, int width, int height, Color tint)
    {
        if (!m_valid)
            return;

        const Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(m_stream.videoTexture.width),
            static_cast<float>(m_stream.videoTexture.height)};
        const Rectangle dest{
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(width),
            static_cast<float>(height)};
        const Vector2 origin{0.0f, 0.0f};

        DrawTexturePro(m_stream.videoTexture, source, dest, origin, 0.0f, tint);
    }

    inline void VideoPlayer::SetLooping(bool loop)
    {
        if (!m_valid)
            return;

        SetMediaLooping(m_stream, loop);
    }

    inline void VideoPlayer::SetVolume(float vol)
    {
        m_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                      : vol;

        if (m_valid)
        {
            SetAudioStreamVolume(m_stream.audioStream, m_volume);
        }
    }

    inline bool VideoPlayer::Seek(double timeSec)
    {
        if (!m_valid)
            return false;

        return SetMediaPosition(m_stream, timeSec);
    }

    inline double VideoPlayer::GetPosition() const
    {
        if (!m_valid)
            return -1.0;

        return GetMediaPosition(m_stream);
    }

    inline double VideoPlayer::GetDuration() const
    {
        if (!m_valid)
            return 0.0;

        return GetMediaProperties(m_stream).durationSec;
    }

    inline int VideoPlayer::GetWidth() const
    {
        if (!m_valid)
            return 0;

        return m_stream.videoTexture.width;
    }

    inline int VideoPlayer::GetHeight() const
    {
        if (!m_valid)
            return 0;

        return m_stream.videoTexture.height;
    }

    inline bool VideoPlayer::IsFinished() const
    {
        return m_finished;
    }

    inline bool VideoPlayer::IsValid() const
    {
        return m_valid;
    }

    inline bool VideoPlayer::IsPlaying() const
    {
        return m_playing;
    }

    inline bool VideoPlayer::IsPaused() const
    {
        if (!m_valid)
            return false;

        return GetMediaState(m_stream) == MEDIA_STATE_PAUSED;
    }

} // namespace video
