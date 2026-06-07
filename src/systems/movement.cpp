#include "movement.h"
#include "player.h"
#include "mapLogic.h"
#include "screen.h"
#include "input.h"
#include "animation.h"
#include "game_debug.h"
#include "effects.h"
#include "../lib/raylib/include/raymath.h"
#include <cmath>

namespace Movement
{
    /**
     * Memperbarui kecepatan dan posisi pemain berdasarkan input.
     * Mengimplementasikan "sliding collision" untuk mencegah pemain tertahan di tembok saat bergerak diagonal.
     */
    void HandleMovement(Player &player)
    {
        player.Velocity = {0, 0};
        bool moving = false;
        Direction nextDir = player.Anim.direction;

        HandleDash(player);

        // Mengambil vektor input mentah
        if (InputInstance.IsMoveUp())
        {
            player.Velocity.y -= 1;
            if (!player.Anim.isAttacking) nextDir = UP;
            moving = true;
        }
        if (InputInstance.IsMoveDown())
        {
            player.Velocity.y += 1;
            if (!player.Anim.isAttacking) nextDir = DOWN;
            moving = true;
        }
        if (InputInstance.IsMoveLeft())
        {
            player.Velocity.x -= 1;
            if (!player.Anim.isAttacking) nextDir = LEFT;
            moving = true;
        }
        if (InputInstance.IsMoveRight())
        {
            player.Velocity.x += 1;
            if (!player.Anim.isAttacking) nextDir = RIGHT;
            moving = true;
        }
        player.IsMoving = moving;

        if (InputInstance.IsRightClickPressed() && player.DashCooldown <= 0.0f && !player.IsDashing && player.Mana < player.DashManaCost)
        {
            TraceLog(LOG_WARNING, "DASH: Mana tidak cukup! Mana: %.2f, Cost: %.2f", player.Mana, player.DashManaCost);
            Effects::AddLog("Stamina tidak cukup!");
            return;
        }

        // Memperbarui status animasi berdasarkan status pergerakan
        ::State nextState = moving ? WALK : IDLE;
        PlayAnimation(player.Anim, nextState, nextDir);

        // Normalisasi kecepatan untuk kecepatan yang konsisten di semua arah (perbaikan diagonal)
        float length = sqrtf(player.Velocity.x * player.Velocity.x + player.Velocity.y * player.Velocity.y);
        if (length != 0)
        {
            player.Velocity.x /= length;
            player.Velocity.y /= length;
        }

        // --- Logika Sliding Collision ---
        // Memeriksa sumbu X dan Y secara independen. Jika salah satu terhalang, sumbu lainnya masih bisa bergerak.
        float totalSpeed = player.Speed + player.DashSpeed;
        Vector2 nextX = {player.Position.x + player.Velocity.x * totalSpeed, player.Position.y};
        Vector2 nextY = {player.Position.x, player.Position.y + player.Velocity.y * totalSpeed};

        if (CanMove(player, nextX))
        {
            player.Position.x = nextX.x;
        }
        if (CanMove(player, nextY))
        {
            player.Position.y = nextY.y;
        }

        player.Anim.position = player.Position;
    }

    /**
     * @brief Memproses dash pemain, termasuk trigger, durasi, cooldown, dan deselerasi.
     * @param player Referensi player yang state dash-nya diperbarui
     */
    void HandleDash(Player &player)
    {
        float dt = Time::DELTA_TIME;

        if (InputInstance.IsRightClickPressed() && player.DashCooldown <= 0.0f && !player.IsDashing && player.Mana >= player.DashManaCost && player.IsMoving)
        {
            TraceLog(LOG_INFO, "DASH: mana check, mana: %.2f, cost: %.2f", player.Mana, player.DashManaCost);

            player.IsDashing = true;
            player.DashDuration = player.DashDurationMax;
            player.DashSpeed = player.DashMaxSpeed;
            player.Mana -= player.DashManaCost;
            player.ManaRegenTimer = player.ManaRegenDelay;
        }

        if (player.IsDashing)
        {
            player.DashDuration -= dt;
            if (player.DashDuration <= 0.0f)
            {
                player.IsDashing = false;
                player.DashDuration = 0.0f;
                player.DashCooldown = player.DashCooldownMax;
            }
        }

        if (player.DashCooldown > 0.0f)
            player.DashCooldown -= dt;

        // deselerasi selalu jalan, selama IsDashing DashSpeed tetap max karena di-set ulang tiap trigger
        if (!player.IsDashing)
        {
            player.DashSpeed = Lerp(player.DashSpeed, 0.0f, player.DashDecel);
            if (player.DashSpeed < 0.1f)
                player.DashSpeed = 0.0f;
        }
    }

    /**
     * Memperbarui posisi dan zoom kamera.
     * Membatasi kamera pada batas map untuk mencegah melihat ke area kosong (void).
     */
    void UpdateCamera(Player &player)
    {
        float mapW = (float)tilesonMap->width * FRAME_SIZE;
        float mapH = (float)tilesonMap->height * FRAME_SIZE;

        // Zoom Dinamis: Perkecil zoom sedikit untuk map yang sangat kecil agar muat di layar
        int minMapTileZoom = 15;
        float autoZoom = (float)GameScreenWidth / (minMapTileZoom * FRAME_SIZE);
        float fixedZoom = 2.0f;
        float cameraZoom = (tilesonMap->width <= minMapTileZoom || tilesonMap->height <= minMapTileZoom)
                               ? autoZoom
                               : fixedZoom;

        if (!isDebugMode)
            camera.zoom = cameraZoom;

        // Menargetkan titik tengah pemain
        camera.target.x = player.Position.x + (FRAME_SIZE / 2.0f);
        camera.target.y = player.Position.y + (FRAME_SIZE / 2.0f);

        // Ukuran viewport kamera dalam koordinat dunia
        float halfW = (GameScreenWidth / 2.0f) / camera.zoom;
        float halfH = (GameScreenHeight / 2.0f) / camera.zoom;

        // Membatasi target kamera pada tepi map
        if (mapW <= halfW * 2.0f)
            camera.target.x = mapW / 2.0f; // Menengahkan kamera jika map lebih kecil dari viewport
        else
        {
            if (camera.target.x < halfW)
                camera.target.x = halfW;
            if (camera.target.x > mapW - halfW)
                camera.target.x = mapW - halfW;
        }

        if (mapH <= halfH * 2.0f)
            camera.target.y = mapH / 2.0f;
        else
        {
            if (camera.target.y < halfH)
                camera.target.y = halfH;
            if (camera.target.y > mapH - halfH)
                camera.target.y = mapH - halfH;
        }
    }

    /**
     * Memvalidasi apakah posisi yang diusulkan aman untuk pemain.
     * Memeriksa batas dunia, tile tabrakan persegi panjang, dan bentuk poligon.
     */
    bool CanMove(Player &player, Vector2 newPosition)
    {
        Rectangle hitbox = BuildHitbox(newPosition, player.GetHitboxOffsetX(), player.GetHitboxOffsetY(), player.GetHitboxWidth(), player.GetHitboxHeight());

        // 1. Pemeriksaan batas dunia (World Boundary)
        if (!IsWithinWorldBounds(hitbox, tilesonMap->width * FRAME_SIZE, tilesonMap->height * FRAME_SIZE))
            return false;

        // 2. Pemeriksaan layer tabrakan persegi panjang Tiled
        if (CheckCollisionAgainstRects(hitbox, player.CollisionRects))
            return false;

        // 3. Pemeriksaan tabrakan poligon/objek Tiled
        if (CheckCollisionAgainstPolygons(hitbox, player.CollisionPolygons))
            return false;

        // 4. Pemeriksaan dynamic obstacles (object runtime seperti bomb)
        if (CheckCollisionAgainstRects(hitbox, DynamicObstacles))
            return false;

        return true;
    }
}
