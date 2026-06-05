#pragma once

/**
 * @file game_debug.h
 * @brief Debug System Module
 *
 * Header ini mendeklarasikan sistem debug untuk menampilkan
 * panel informasi runtime dan overlay debug di world space.
 */

#include "../lib/raylib/include/raylib.h"
#include "enemy.h"
#include <string>
#include <vector>

/*==============================================================================
 * Debug Class
 *==============================================================================*/

/**
 * @brief Menangani seluruh fitur panel dan overlay debug
 *
 * Sistem ini dipakai untuk menampilkan informasi runtime seperti:
 * - Info map
 * - Info camera
 * - Info player
 * - Info collision
 * - Info frustum culling
 * - Overlay debug di world space
 */
class Debug
{
public:
    /** @brief Toggle debug mode berdasarkan input */
    void Toggle(void);

    /** @brief Render seluruh panel debug yang aktif */
    void Draw(void);

    /** @brief Render overlay debug langsung di world space */
    void DrawWorldOverlay(void);

    /** @brief Render overlay steering behavior untuk enemy */
    static void DrawSteeringOverlay(Enemy &enemy);

private:
    /*==========================================================================
     * Internal Structs
     *==========================================================================*/

    /** @brief Menyimpan data satu panel debug */
    struct DebugPanelEntry
    {
        std::string name;                        // Nama panel
        void (Debug::*drawFn)(Rectangle bounds); // Pointer ke fungsi render panel
        bool enabled;                            // Status panel aktif atau tidak
    };

    /*==========================================================================
     * Private Helper Methods
     *==========================================================================*/

    Rectangle GetPanelBounds(int index, float panelWidth, float panelHeight) const; // Hitung posisi dan ukuran panel
    std::vector<DebugPanelEntry> BuildActivePanels(void) const;                     // Bangun daftar panel yang aktif

    void DrawCollisionOverlay(const std::string &layerName, Color rectColor, Color polygonColor, Color pointColor); // Overlay collision
    void DrawRaycastOverlay(void);                                                                                  // Overlay raycast
    void DrawAttackOverlay(void);                                                                                   // Overlay attack
    void DrawEnemySpawnOverlay(void);                                                                               // Overlay enemy spawn
    void DrawFlowFieldOverlay(const FlowField &field);                                                              // Overlay flow field
    void DrawPanelFrame(Rectangle bounds, const char *title, Color borderColor) const;                              // Frame panel debug

    /*==========================================================================
     * Debug Panels
     *==========================================================================*/

    void DrawMapPanel(Rectangle bounds);       // Panel info map
    void DrawCameraPanel(Rectangle bounds);    // Panel info camera
    void DrawPlayerPanel(Rectangle bounds);    // Panel info player
    void DrawZoomPanel(Rectangle bounds);      // Panel zoom debug
    void DrawFrustumPanel(Rectangle bounds);   // Panel info frustum culling
    void DrawCollisionPanel(Rectangle bounds); // Panel info collision dan boundary

    // void DebugMouse(GameState *state); // sementara tidak dipakai
};

/*==============================================================================
 * Global Debug Instance
 *==============================================================================*/

/** @brief Global instance debug */
extern Debug DebugInstance;

/** @brief Flag status debug mode */
extern bool isDebugMode;
/** @brief Flag toggle overlay flow field enemy */
extern bool showFlowFieldOverlay;
/** @brief Flag toggle overlay flow field player */
extern bool showFlowFieldOverlayPlayer;
