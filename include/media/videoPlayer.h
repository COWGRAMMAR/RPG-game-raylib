#pragma once

#include <string>
#include <cstring>
#include <vector>
#include "raylib.h"
#include <mpv/client.h>
#include <mpv/render.h>

namespace video
{

    class VideoPlayer
    {
    public:
        VideoPlayer() = default;
        ~VideoPlayer();

        bool Load(const std::string &filePath);
        void Unload();

        void Play();
        void Pause();
        void Stop();

        void Update(float deltaTime);
        void Draw(int x, int y, int width, int height, Color tint = WHITE);

        void SetLooping(bool loop);
        void SetVolume(float vol);
        bool Seek(double timeSec);

        double GetPosition() const;
        double GetDuration() const;
        int GetWidth() const;
        int GetHeight() const;

        bool IsFinished() const;
        bool IsValid() const;
        bool IsPlaying() const;
        bool IsPaused() const;

    private:
        mpv_handle *m_mpv{nullptr};
        mpv_render_context *m_mpv_gl{nullptr};
        RenderTexture2D m_renderTarget{};
        std::vector<uint8_t> m_swBuffer;
        int m_width{};
        int m_height{};
        float m_volume{1.0f};
        bool m_valid{};
        bool m_finished{};
        bool m_playing{};

        static void on_mpv_update(void *ctx);
        void handle_events();
        void ensure_render_target(int w, int h);
    };

    // =============================================================================
    // Destructor / Load / Unload
    // =============================================================================

    inline VideoPlayer::~VideoPlayer()
    {
        Unload();
    }

    inline bool VideoPlayer::Load(const std::string &filePath)
    {
        Unload();

        m_mpv = mpv_create();
        if (!m_mpv)
            return false;

        mpv_set_option_string(m_mpv, "vo", "libmpv");
        mpv_set_option_string(m_mpv, "keep-open", "no");
        mpv_set_option_string(m_mpv, "cache", "yes");
        mpv_set_option_string(m_mpv, "volume", "100");

        if (mpv_initialize(m_mpv) < 0)
        {
            mpv_destroy(m_mpv);
            m_mpv = nullptr;
            return false;
        }

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_SW},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        if (mpv_render_context_create(&m_mpv_gl, m_mpv, params) < 0)
        {
            mpv_destroy(m_mpv);
            m_mpv = nullptr;
            return false;
        }

        mpv_render_context_set_update_callback(m_mpv_gl, on_mpv_update, this);
        mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

        // Must be set before loadfile to capture decoder/renderer init messages
        mpv_request_log_messages(m_mpv, "debug");

        const char *cmd[] = {"loadfile", filePath.c_str(), nullptr};
        int r = mpv_command(m_mpv, cmd);
        if (r < 0)
        {
            TraceLog(LOG_WARNING, "MPV: loadfile returned %d for '%s'", r, filePath.c_str());
            mpv_destroy(m_mpv);
            m_mpv = nullptr;
            return false;
        }

        m_valid = true;
        return true;
    }

    inline void VideoPlayer::Unload()
    {
        m_valid = false;
        m_finished = false;
        m_playing = false;
        m_width = 0;
        m_height = 0;

        if (m_renderTarget.id)
        {
            UnloadRenderTexture(m_renderTarget);
            m_renderTarget = {};
        }

        if (m_mpv_gl)
        {
            mpv_render_context_free(m_mpv_gl);
            m_mpv_gl = nullptr;
        }

        if (m_mpv)
        {
            mpv_terminate_destroy(m_mpv);
            m_mpv = nullptr;
        }
    }

    // =============================================================================
    // Playback Control
    // =============================================================================

    inline void VideoPlayer::Play()
    {
        if (!m_valid)
            return;

        mpv_set_property_string(m_mpv, "pause", "no");
        m_playing = true;
        m_finished = false;
    }

    inline void VideoPlayer::Pause()
    {
        if (!m_valid)
            return;

        mpv_set_property_string(m_mpv, "pause", "yes");
        m_playing = false;
    }

    inline void VideoPlayer::Stop()
    {
        if (!m_valid)
            return;

        mpv_set_property_string(m_mpv, "pause", "yes");
        const char *cmd[] = {"seek", "0", "absolute", nullptr};
        mpv_command(m_mpv, cmd);
        m_playing = false;
        m_finished = false;
    }

    // =============================================================================
    // Per-Frame Update and Render
    // =============================================================================

    inline void VideoPlayer::Update(float deltaTime)
    {
        static_cast<void>(deltaTime);
        if (!m_valid)
            return;

        handle_events();

        if (!m_width || !m_height || !m_renderTarget.id)
            return;

        int sw_size[2] = {m_width, m_height};
        const char *sw_fmt = "rgb0";
        size_t sw_stride = (size_t)m_width * 4;

        // Ensure buffer is large enough (extra padding for last line overrun per mpv docs)
        size_t needed = sw_stride * (size_t)m_height + 64;
        if (m_swBuffer.size() < needed)
            m_swBuffer.resize(needed, 0);

        mpv_render_param render_params[] = {
            {MPV_RENDER_PARAM_SW_SIZE, sw_size},
            {MPV_RENDER_PARAM_SW_FORMAT, (void *)sw_fmt},
            {MPV_RENDER_PARAM_SW_STRIDE, &sw_stride},
            {MPV_RENDER_PARAM_SW_POINTER, m_swBuffer.data()},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        int renderRet = mpv_render_context_render(m_mpv_gl, render_params);

        // Fix alpha channel: mpv "rgb0" leaves 4th byte uninitialized
        {
            uint8_t *pixels = m_swBuffer.data();
            for (int y = 0; y < m_height; y++)
            {
                uint8_t *row = pixels + y * sw_stride;
                for (int x = 0; x < m_width; x++)
                    row[x * 4 + 3] = 255;
            }
        }

        UpdateTexture(m_renderTarget.texture, m_swBuffer.data());

        TraceLog(LOG_INFO, "MPV: sw_render_ret=%d", renderRet);
    }

    inline void VideoPlayer::Draw(int x, int y, int width, int height, Color tint)
    {
        if (!m_valid || !m_renderTarget.id)
            return;

        float texW = (float)m_renderTarget.texture.width;
        float texH = (float)m_renderTarget.texture.height;

        // SW-rendered data is top-to-bottom, no flip needed
        Rectangle source = {0.0f, 0.0f, texW, texH};
        Rectangle dest = {
            (float)x, (float)y,
            (float)width, (float)height
        };
        Vector2 origin = {0.0f, 0.0f};

        DrawTexturePro(m_renderTarget.texture, source, dest, origin, 0.0f, tint);
    }

    // =============================================================================
    // Configuration
    // =============================================================================

    inline void VideoPlayer::SetLooping(bool loop)
    {
        if (!m_valid)
            return;

        mpv_set_property_string(m_mpv, "loop-file", loop ? "inf" : "no");
    }

    inline void VideoPlayer::SetVolume(float vol)
    {
        m_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f
                                                       : vol;

        if (m_valid)
        {
            int mpvVol = (int)(m_volume * 100.0f);
            mpv_set_property(m_mpv, "volume", MPV_FORMAT_INT64, &mpvVol);
        }
    }

    inline bool VideoPlayer::Seek(double timeSec)
    {
        if (!m_valid)
            return false;

        std::string t = std::to_string(timeSec);
        const char *cmd[] = {"seek", t.c_str(), "absolute", nullptr};
        return mpv_command(m_mpv, cmd) >= 0;
    }

    // =============================================================================
    // Query
    // =============================================================================

    inline double VideoPlayer::GetPosition() const
    {
        if (!m_valid)
            return -1.0;

        double pos;
        if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0)
            return pos;
        return -1.0;
    }

    inline double VideoPlayer::GetDuration() const
    {
        if (!m_valid)
            return 0.0;

        double dur;
        if (mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &dur) >= 0)
            return dur;
        return 0.0;
    }

    inline int VideoPlayer::GetWidth() const
    {
        return m_width;
    }

    inline int VideoPlayer::GetHeight() const
    {
        return m_height;
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

        int paused = 0;
        mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused);
        return paused != 0;
    }

    // =============================================================================
    // Private Helpers
    // =============================================================================

    inline void VideoPlayer::on_mpv_update(void *ctx)
    {
        static_cast<void>(ctx);
    }

    inline void VideoPlayer::handle_events()
    {
        while (m_mpv)
        {
            mpv_event *ev = mpv_wait_event(m_mpv, 0);
            if (ev->event_id == MPV_EVENT_NONE)
                break;

            switch (ev->event_id)
            {
            case MPV_EVENT_LOG_MESSAGE:
            {
                auto *log = (mpv_event_log_message *)ev->data;
                TraceLog(LOG_INFO, "MPV [%s] %s", log->prefix, log->text);
                break;
            }
            case MPV_EVENT_VIDEO_RECONFIG:
            {
                int64_t w = 0, h = 0;
                mpv_get_property(m_mpv, "width", MPV_FORMAT_INT64, &w);
                mpv_get_property(m_mpv, "height", MPV_FORMAT_INT64, &h);
                TraceLog(LOG_INFO, "MPV: VIDEO_RECONFIG w=%lld h=%lld", (long long)w, (long long)h);
                if (w > 0 && h > 0)
                    ensure_render_target((int)w, (int)h);
                break;
            }
            case MPV_EVENT_END_FILE:
            {
                auto *end = (mpv_event_end_file *)ev->data;
                TraceLog(LOG_INFO, "MPV: END_FILE reason=%d", end->reason);
                m_finished = true;
                m_playing = false;
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                auto *prop = (mpv_event_property *)ev->data;
                if (strcmp(prop->name, "eof-reached") == 0
                    && prop->format == MPV_FORMAT_FLAG
                    && prop->data)
                {
                    int eof = *(int *)prop->data;
                    if (eof)
                    {
                        m_finished = true;
                        m_playing = false;
                    }
                }
                break;
            }
            default:
                TraceLog(LOG_INFO, "MPV: event %d (%s)", ev->event_id, mpv_event_name(ev->event_id));
                break;
            }
        }
    }

    inline void VideoPlayer::ensure_render_target(int w, int h)
    {
        if (m_renderTarget.id && m_width == w && m_height == h)
            return;

        if (m_renderTarget.id)
            UnloadRenderTexture(m_renderTarget);

        m_renderTarget = LoadRenderTexture(w, h);
        m_width = w;
        m_height = h;
    }

} // namespace video
