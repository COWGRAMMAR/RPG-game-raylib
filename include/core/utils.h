#pragma once

/**
 * @file utils.h
 * @brief Shared utility functions
 *
 * Provides common helper functions used across the codebase.
 */

#include <string>
#include <sstream>
#include <iomanip>
#include <random>

/**
 * @brief Generate a random UUID string.
 *
 * Returns a 32-character hex string (e.g., "a1b2c3d4e5f6789012345678abcdef01")
 * using std::random_device and std::mt19937 for entropy.
 * Used to assign unique persistent identifiers to entities at spawn time
 * for reliable matching across save/load cycles.
 *
 * Marked inline to avoid ODR violations when included from multiple translation units.
 *
 * @return std::string Random 32-char hex string
 */
inline std::string GenerateUUID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(0, 255);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; i++)
        ss << std::setw(2) << dist(gen);
    return ss.str();
}

/**
 * @brief Generate deterministic UUID dari seed + context.
 *
 * Berbeda dengan GenerateUUID() yang random, fungsi ini menghasilkan UUID
 * deterministic dari kombinasi seed, mapObjectID, name, dan instanceIndex.
 * Dipakai untuk entity matching di save/load dan restart — spawn selalu
 * produce UUID yang sama kalo inputnya sama.
 *
 * @param seed Seed dari SeedManager (0 kalo non-worldgen)
 * @param mapObjectID ID object spawn dari Tiled
 * @param name Nama entity/item
 * @param instanceIndex Index instance dalam satu spawn point (default 0)
 * @return std::string Hex string deterministic
 */
inline std::string GenerateDeterministicUUID(uint64_t seed, int mapObjectID,
                                              const std::string &name,
                                              int instanceIndex = 0)
{
    std::hash<std::string> hasher;
    std::string input = std::to_string(seed) + "_"
                      + std::to_string(mapObjectID) + "_"
                      + name + "_"
                      + std::to_string(instanceIndex);
    std::size_t hash = hasher(input);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}
