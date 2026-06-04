/**
 * @file seedmanager.cpp
 * @brief Implementasi Seed Manager System
 *
 * File ini berisi implementasi manajemen seed untuk world generation:
 * - InitRun: generate seed array untuk tiap stage
 * - GetSeed: akses seed per stage
 * - SaveMeta / LoadMeta: persistensi seed & state ke file JSON
 */

#include "seedmanager.h"
#include <nlohmann/json.hpp>

SeedManager g_SeedManager;
#include <fstream>
#include <random>
#include <filesystem>

/** @brief Generate seed untuk tiap stage dan init run state */
void SeedManager::InitRun(int saveSlot)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;

    for (int i = 0; i < SEED_COUNT; i++)
        seeds[i] = dist(gen);

    currentStage = 0;
    currentSlot = saveSlot;
    isRunActive = true;
}

/** @brief Dapatkan seed untuk stage tertentu */
uint32_t SeedManager::GetSeed(int stage) const
{
    if (stage < 0 || stage >= SEED_COUNT)
        return 0;
    return seeds[stage];
}

/** @brief Simpan seed & state run ke file meta.json */
void SeedManager::SaveMeta(const std::string &filePath)
{
    nlohmann::json j;
    j["seeds"] = nlohmann::json::array();
    for (int i = 0; i < SEED_COUNT; i++)
        j["seeds"].push_back(seeds[i]);
    j["currentStage"] = currentStage;
    j["prevStage"] = prevStage;
    j["currentSlot"] = currentSlot;

    std::ofstream file(filePath);
    if (file.is_open())
        file << j.dump(4);
}

/** @brief Load seed & state run dari file meta.json */
bool SeedManager::LoadMeta(const std::string &filePath)
{
    namespace fs = std::filesystem;
    if (!fs::exists(filePath))
        return false;

    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    nlohmann::json j;
    try
    {
        file >> j;
        auto seedsArr = j["seeds"];
        for (int i = 0; i < SEED_COUNT && i < (int)seedsArr.size(); i++)
            seeds[i] = seedsArr[i];
        currentStage = j.value("currentStage", 0);
        prevStage = j.value("prevStage", -1);
        currentSlot = j.value("currentSlot", 1);
        isRunActive = true;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
