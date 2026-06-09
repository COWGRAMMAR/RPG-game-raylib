/**
 * @file entities.cpp
 * @brief Implementasi Entity Registry System
 *
 * File ini berisi implementasi registri global untuk entity management:
 * - Registry / DynamicRegistry untuk entity lifecycle
 * - EnemyRegistry untuk akses enemy
 * - DeadEntities tracking untuk respawn prevention
 * - RenderTileProps / ClearTileProps untuk tile-based props
 */

#include "entities.h"
#include "../../include/map/map.h"
#include "propsbehavior.h"
#include <vector>
#include <algorithm>
#include <set>
#include <string>

namespace Entities
{
    /**
     * @brief Daftar utama semua entitas yang saat ini ada di dalam scene (statis dan dinamis).
     */
    static std::vector<Entity *> Registry;

    /**
     * @brief Daftar entitas yang dimiliki oleh registri (dialokasikan secara dinamis).
     * Entitas-entitas ini akan dihapus saat Clear() atau Shutdown() dipanggil.
     */
    static std::vector<Entity *> DynamicRegistry;

    /** @brief Registri global enemy */
    std::vector<Enemy *> EnemyRegistry;

    /**
     * @brief Set ID unik (map + indeks objek) untuk entitas yang tidak boleh muncul kembali (respawn).
     */
    static std::set<std::string> DeadEntities;

    /**
     * @brief Set UUID per-instance enemy yang sudah mati.
     *
     * Format key: mapPath + "_uuid_" + uuid
     * Setiap enemy punya UUID unik, jadi kematian satu enemy
     * tidak mematikan seluruh spawn point (beda dengan MapObjectID).
     */
    static std::set<std::string> DeadEntitiesByUUID;

    void Init()
    {
        Registry.clear();
        DynamicRegistry.clear();
    }

    /**
     * Mengirimkan panggilan Update ke semua entitas yang aktif.
     */
    void Update()
    {
        for (size_t i = 0; i < Registry.size(); i++)
        {
            if (Registry[i] && Registry[i]->IsActive)
            {
                Registry[i]->Update();
            }
        }
    }

    /**
     * Mengurutkan entitas berdasarkan koordinat Y sebelum di-render untuk mencapai kedalaman pseudo-3D (Y-sorting).
     */
    int Render(Rectangle viewRect)
    {
        std::vector<Entity *> visible;
        visible.reserve(Registry.size());
        for (auto entity : Registry)
        {
            if (entity && entity->IsActive && CheckCollisionRecs(entity->GetHitbox(), viewRect))
                visible.push_back(entity);
        }

        std::sort(visible.begin(), visible.end(), [](Entity *a, Entity *b)
                  { return a->Position.y < b->Position.y; });

        for (auto entity : visible)
            entity->Render();

        return (int)visible.size();
    }

    /**
     * Membersihkan semua entitas yang dikelola.
     */
    void Shutdown()
    {
        Clear();
    }

    /**
     * Mendaftarkan entitas yang sudah ada sebelumnya (contoh: Player) tanpa mengambil kepemilikan memori.
     */
    void Add(Entity *entity)
    {
        if (entity)
        {
            Registry.push_back(entity);
        }
    }

    /**
     * Menambahkan entitas dan mengambil alih kepemilikan memorinya.
     */
    void AddDynamic(Entity *entity)
    {
        if (entity)
        {
            DynamicRegistry.push_back(entity);
            Registry.push_back(entity);

            Enemy *e = dynamic_cast<Enemy *>(entity);
            if (e)
                EnemyRegistry.push_back(e);
        }
    }

    /**
     * Membersihkan scene saat ini. Menghapus semua entitas dinamis.
     */
    void Clear()
    {
        for (auto entity : DynamicRegistry)
        {
            delete entity;
        }
        DynamicRegistry.clear();
        EnemyRegistry.clear();
        Registry.clear();
    }

    /**
     * Mengembalikan daftar entitas global untuk pemeriksaan tabrakan/interaksi.
     */
    const std::vector<Entity *> &GetRegistry()
    {
        return Registry;
    }

    /** @brief Getter EnemyRegistry */
    std::vector<Enemy *> &GetEnemyRegistry()
    {
        return EnemyRegistry;
    }

    /**
     * Menandai entitas sebagai mati untuk mencegahnya muncul kembali saat map dimuat ulang.
     */
    void RegisterDeath(const std::string &mapPath, int objectId)
    {
        DeadEntities.insert(mapPath + "_" + std::to_string(objectId));
    }

    /**
     * Memeriksa apakah objek map tertentu sudah tercatat sebagai mati.
     */
    bool IsAlreadyDead(const std::string &mapPath, int objectId)
    {
        return DeadEntities.find(mapPath + "_" + std::to_string(objectId)) != DeadEntities.end();
    }

    /**
     * @brief Catat kematian enemy per-instance berdasarkan UUID
     * @param mapPath Path map tempat enemy mati
     * @param uuid UUID unik per-instance enemy
     */
    void RegisterDeathByUUID(const std::string &mapPath, const std::string &uuid)
    {
        DeadEntitiesByUUID.insert(mapPath + "_uuid_" + uuid);
        TraceLog(LOG_INFO, "REGISTER_DEATH_UUID: map='%s' uuid='%s'", mapPath.c_str(), uuid.c_str());
    }

    /**
     * @brief Cek apakah enemy dengan UUID tertentu sudah mati
     * @param mapPath Path map
     * @param uuid UUID unik per-instance enemy
     * @return true jika sudah mati
     */
    bool IsDeadByUUID(const std::string &mapPath, const std::string &uuid)
    {
        return DeadEntitiesByUUID.find(mapPath + "_uuid_" + uuid) != DeadEntitiesByUUID.end();
    }

    void ClearDeadEntities()
    {
        DeadEntities.clear();
        DeadEntitiesByUUID.clear();
    }

    const std::set<std::string> &GetDeadEntities()
    {
        return DeadEntities;
    }

    void SetDeadEntities(const std::set<std::string> &entities)
    {
        DeadEntities = entities;
    }

    /** @brief Safety pass: deactivate any EnemyRegistry entries registered as dead. */
    void PruneDeadEntities(void)
    {
        std::string currentMap = GetCurrentMapPath();
        int pruned = 0;
        for (auto &enemy : EnemyRegistry)
        {
            if (enemy == nullptr) continue;
            if (IsDeadByUUID(currentMap, enemy->GetUUID()))
            {
                enemy->IsActive = false;
                enemy->Health = 0.0f;
                TraceLog(LOG_WARNING, "PRUNE: Deactivated dead enemy '%s' (UUID=%s) that survived spawn",
                         enemy->Name.c_str(), enemy->GetUUID().c_str());
                pruned++;
            }
        }
        if (pruned > 0)
            TraceLog(LOG_INFO, "PRUNE: Deactivated %d dead entit%s that survived spawn", pruned, pruned == 1 ? "y" : "ies");
    }
}

/** @brief Render semua tile-based props yang visible */
void RenderTileProps(Rectangle viewRect)
{
    int chestRendered = chestManager.Render(viewRect);
    int spikeRendered = spikeManager.Render(viewRect);
    int bombRendered = bombManager.Render(viewRect);
    int crateRendered = crateManager.Render(viewRect);
    barrierManager.Render(viewRect);
    signManager.Render(viewRect);
}

/** @brief Clear semua state tile-based props */
void ClearTileProps(void)
{
    chestManager.Clear();
    spikeManager.Clear();
    bombManager.Clear();
    crateManager.Clear();
    barrierManager.Clear();
    signManager.Clear();
}
