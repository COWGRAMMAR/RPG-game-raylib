#pragma once

/**
 * @file videoScreen.h
 * @brief Layar untuk memutar video intro saat startup game.
 */

#include "raylib.h"
#include "../media/videoPlayer.h"
#include <string>

class VideoScreen
{
public:
    VideoScreen();
    ~VideoScreen();

    /**
     * @brief Perbarui state video setiap frame
     * @param deltaTime Waktu sejak frame terakhir
     * @return true jika video selesai atau di-skip (layar harus pindah ke MAIN_MENU)
     */
    bool Update(float deltaTime);

    /**
     * @brief Render frame video saat ini
     */
    void Draw();

    /**
     * @brief Set path file video yang akan diputar
     * @param path Path ke file video (relatif terhadap root project)
     */
    void SetVideoPath(const std::string& path);

    /**
     * @brief Dapatkan path file video saat ini
     * @return const std::string&
     */
    const std::string& GetVideoPath() const;

    /**
     * @brief Muat dan mulai putar video
     * @return true jika video berhasil dimuat
     */
    bool LoadAndPlay();

    /**
     * @brief Set volume playback video (0.0f - 1.0f)
     */
    void SetVolume(float vol);

    /**
     * @brief Hentikan dan bersihkan video
     */
    void Unload();

private:
    video::VideoPlayer m_player;       ///< Player video
    std::string        m_videoPath;    ///< Path file video
    bool               m_skipRequested;///< true jika user minta skip
    bool               m_loaded;       ///< true jika video sudah dimuat
};
