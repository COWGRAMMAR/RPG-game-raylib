#pragma once
#include <set>
#include <vector>
#include <string>
#include <set>
#include "entity.h"
#include "enemy.h"

/**
 * @file entities.h
 * @brief Entity Registry & Management Module
 *
 * Header ini mendeklarasikan sistem registri dan manajemen global
 * untuk entitas: init, update, render, persistensi kematian.
 */

/**
 * @brief Sistem registri dan manajemen global untuk entitas
 *
 * Menangani inisialisasi, update, render, dan persistensi status kematian.
 */
namespace Entities
{
    /** @brief Daftar semua enemy aktif */
    extern std::vector<Enemy *> EnemyRegistry;
    /** @brief Inisialisasi sistem entitas */
    void Init();
    /** @brief Update semua entitas yang aktif */
    void Update();
    /** @brief Render semua entitas yang aktif */
    int Render(Rectangle viewRect);
    /** @brief Bersihkan semua entitas */
    void Shutdown();

    /** @brief Tambah entitas statis/persisten ke registri */
    void Add(Entity *entity);

    /** @brief Tambah entitas dinamis (efek sementara, proyektil) */
    void AddDynamic(Entity *entity);

    /** @brief Hapus semua entitas dari registri */
    void Clear();

    /** @brief Dapatkan daftar semua entitas yang terdaftar */
    const std::vector<Entity *> &GetRegistry();

    /**
     * @brief Catat entitas sebagai mati di map tertentu (cegah respawn)
     * @param mapPath Path map tempat entitas mati
     * @param objectId ID object Tiled
     */
    void RegisterDeath(const std::string &mapPath, int objectId);

    /**
     * @brief Cek apakah entitas sudah pernah dibunuh di suatu map
     * @param mapPath Path map
     * @param objectId ID object Tiled
     * @return true jika sudah mati sebelumnya
     */
    bool IsAlreadyDead(const std::string &mapPath, int objectId);

    /**
     * @brief Membersihkan semua catatan entitas mati (untuk new game).
     */
    void ClearDeadEntities();

    /** @brief Ambil reference ke enemy registry */
    std::vector<Enemy *> &GetEnemyRegistry();

    /**
     * @brief Mendapatkan referensi ke set entitas yang sudah mati.
     * @return const ref ke DeadEntities set
     */
    const std::set<std::string> &GetDeadEntities();

    /**
     * @brief Mengganti seluruh isi DeadEntities set.
     * @param entities Set entitas mati baru
     */
    void SetDeadEntities(const std::set<std::string> &entities);

    /**
     * @brief Safety net: deactivate any EnemyRegistry entries that are registered as dead.
     * @details Iterates the enemy registry and sets IsActive=false, Health=0 for any enemy
     *          whose (currentMap, MapObjectID) pair exists in the DeadEntities set.
     *          Call AFTER SpawnEnemiesFromMap + RestoreGameState as a safety pass.
     */
    void PruneDeadEntities(void);
}

/** @brief Render tile props (chest, trap, dll) */
void RenderTileProps(Rectangle viewRect);

/** @brief Clear tile props (chest, trap, dll) */
void ClearTileProps(void);
