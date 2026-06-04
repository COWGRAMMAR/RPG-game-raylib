#include "interaction.h"
#include "player.h"
#include "input.h"
#include "mapLogic.h"
#include "map.h"
#include "screen.h"
#include "propsbehavior.h"
#include "seedmanager.h"
#include "worldgenio.h"
#include "entities.h"
#include "../lib/raylib/include/raymath.h"

namespace Interaction
{
    /**
     * Pengendali tingkat atas untuk semua interaksi lingkungan.
     */
    void HandleInteractions(Player &player)
    {
        player.canInteract = false;
        UpdateRaycast(player);
        CheckDoors(player);    // Interaksi berbasis kedekatan (proximity)
        CheckProps(player);    // Interaksi berbasis raycast
    }

    /**
     * Melakukan logika perpindahan map secara aktual.
     * Dipisahkan dari pemeriksaan untuk menghindari korupsi status di tengah update.
     */
    void ExecutePendingTransitions(Player &player)
    {
        if (player.pendingGoBack)
        {
            player.pendingGoBack = false;
            GoBack();
        }

        if (player.pendingSwitchMap)
        {
            player.pendingSwitchMap = false;
            SwitchMap(player.pendingMapPath.c_str(), player.pendingDoorName.c_str());
        }
    }

    /**
     * Memperbarui raycast interaksi pemain.
     * Memproyeksikan ray pendek ke arah mouse untuk mendeteksi objek yang dapat berinteraksi.
     */
    void UpdateRaycast(Player &player)
    {
        Vector2 playerCenter = {
            player.Position.x + player.GetHitboxOffsetX() + player.GetHitboxWidth() / 2,
            player.Position.y + player.GetHitboxOffsetY() + player.GetHitboxHeight() / 2};

        Vector2 mouseWorld = GetScreenToWorld2D(GetVirtualMousePosition(player.State), camera);
        Vector2 aimDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));

        // Memfilter objek untuk hanya memeriksa yang ada di layer Interaction
        std::vector<MapObject> propObjects;
        for (auto &obj : tilesonMap->Objects)
        {
            if (obj.layerName == OBJECT_LAYER_NAME)
                propObjects.push_back(obj);
        }

        player.LastHit = player.Ray.Cast(playerCenter, aimDir, player.GetInteractRange(), propObjects);
    }

    /**
     * Memeriksa apakah pemain berdiri di dalam pemicu (trigger) pintu dan menekan tombol interaksi.
     */
    void CheckDoors(Player &player)
    {
        // Skip kalau lagi transisi map — cegah double trigger dari fixed-timestep loop
        if (gState->isSwitchingMap || gState->isGoingBack)
            return;

        Rectangle playerHitbox = BuildHitbox(player.Position, player.GetHitboxOffsetX(), player.GetHitboxOffsetY(), player.GetHitboxWidth(), player.GetHitboxHeight());
        std::vector<MapObject *> doors = TiledHelperFunction.GetObjectsByType(DOOR_TYPE_OBJECT_NAME);

        for (const auto &door : doors)
        {
            if (!CheckCollisionRecs(playerHitbox, door->bounds))
                continue;

            player.canInteract = true;

            if (!InputInstance.IsInteract())
                continue;

            // Cek worldgen door — property "worldgen":"true" → InitRun + SwitchMap ke stage 1
            auto wgIt = door->properties.find("worldgen");
            if (wgIt != door->properties.end() && wgIt->second.getValue<std::string>() == "true")
            {
                // InitRun cuma sekali per run — kalau sudah aktif, lanjut ke stage terakhir
                if (!g_SeedManager.IsRunActive())
                    WorldgenIO::InitRun(WorldgenIO::GetNextAvailableSlot());

                player.pendingSwitchMap = true;
                player.pendingMapPath = WorldgenIO::GetStagePath(g_SeedManager.GetCurrentStage());
                player.pendingDoorName = "start";
                return;
            }

            // Cek door stage_exit — property "stage_exit":"next" → NextStage (finish door)
            auto stageExitIt = door->properties.find("stage_exit");
            if (stageExitIt != door->properties.end())
            {
                const std::string &val = stageExitIt->second.getValue<std::string>();
                if (val == "next")
                {
                    WorldgenIO::NextStage();
                    return;
                }
                if (val == "prev")
                {
                    WorldgenIO::PrevStage();
                    return;
                }
            }

            // Deteksi door boss — cuma bisa dipake kalo boss udah mati
            if (door->name == "boss")
            {
                bool bossAlive = false;
                for (auto *enemy : Entities::GetEnemyRegistry())
                {
                    if (enemy->rank == ENEMY_BOSS && enemy->IsActive)
                    {
                        bossAlive = true;
                        break;
                    }
                }
                if (!bossAlive)
                {
                    WorldgenIO::SaveRuntimeState(g_SeedManager.GetCurrentStage());
                    g_SeedManager.ResetRun();
                    gState->currentScreen = MAIN_MENU;
                }
                return;
            }

            // Mengambil map tujuan dan ID pintu dari properti Tiled
            auto mapIt = door->properties.find("target_map");
            auto doorIt = door->properties.find("target_door");

            if (mapIt != door->properties.end() && doorIt != door->properties.end())
            {
                player.pendingSwitchMap = true;
                player.pendingMapPath = mapIt->second.getValue<std::string>();
                player.pendingDoorName = doorIt->second.getValue<std::string>();
                return;
            }
        }
    }

    /**
     * Menangani interaksi dengan properti (peti, tuas, dll.) yang terkena raycast.
     * Interaksi hanya bisa dilakukan jika arah aim berada dalam area pandang player (±45°).
     */
    void CheckProps(Player &player)
    {
        if (!player.LastHit.hit)
            return;

        // Cek apakah aim berada dalam area pandang player (garis raycast hijau)
        Vector2 playerCenter = player.GetCenter();
        Vector2 mouseWorld = GetScreenToWorld2D(GetVirtualMousePosition(player.State), camera);
        Vector2 aimDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));

        Vector2 facingDir = {0, 0};
        switch (player.Anim.direction)
        {
            case UP:    facingDir = {0, -1}; break;
            case DOWN:  facingDir = {0, 1};  break;
            case LEFT:  facingDir = {-1, 0}; break;
            case RIGHT: facingDir = {1, 0};  break;
        }

        float dot = Vector2DotProduct(facingDir, aimDir);
        if (dot < player.GetRayCastAngle())
            return; // Di luar area pandang, tidak bisa interaksi

        const std::string &type = player.LastHit.object->type;
        if (type != CHEST_TYPE_OBJECT_NAME)
            return; // Hanya objek tipe CHEST_TYPE_OBJECT_NAME yang bisa diinteraksi via raycast
            
        player.canInteract = true;

        if (!InputInstance.IsInteract())
            return;

        TraceLog(LOG_INFO, "Berinteraksi dengan: '%s' (tipe: %s)", player.LastHit.object->name.c_str(), type.c_str());

        // Percabangan logika berdasarkan tipe objek
        if (type == CHEST_TYPE_OBJECT_NAME)
        {
            TraceLog(LOG_INFO, "Membuka peti: '%s'", player.LastHit.object->name.c_str());
            chestManager.Interact(player.LastHit.point);
        }
    }
}

