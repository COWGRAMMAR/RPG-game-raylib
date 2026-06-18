#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

/*==============================================================================
 * Inventory Dimensions
 *==============================================================================*/
constexpr int HOTBAR_SLOTS   = 4;   /**< Jumlah slot hotbar */
constexpr int BAG_SLOTS      = 12;  /**< Jumlah slot bag */
constexpr int TOTAL_INVENTORY_SLOTS = HOTBAR_SLOTS + BAG_SLOTS; /**< Total inventory */

/*==============================================================================
 * Player Defaults
 *==============================================================================*/
constexpr float DEFAULT_MAX_HEALTH = 100.0f; /**< Default HP maksimum player */
constexpr float DEFAULT_MAX_MANA   = 100.0f; /**< Default Mana maksimum player */

/*==============================================================================
 * Item / Pickup
 *==============================================================================*/
constexpr float SPAWN_IMMUNITY_DURATION = 1.0f; /**< Immunity time after item spawn (Minecraft style) */
constexpr float DROP_IMMUNITY_DURATION  = 5.0f; /**< Immunity time after item dropped */

/*==============================================================================
 * Worldgen
 *==============================================================================*/
constexpr unsigned int WORLDOGEN_SEED_MAGIC = 0xDEADBEEF; /**< XOR mask for deterministic seed */

#endif // GAME_CONSTANTS_H
