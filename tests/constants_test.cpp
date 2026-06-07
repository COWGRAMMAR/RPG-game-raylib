// Ensure Windows types are available (required by doctest), minimize conflicts
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#endif

#define DOCTEST_CONFIG_IMPLEMENT
#include "../lib/doctest/doctest.h"

// Provide WinMain entry point (raylib provides this for the main game, but
// test_constants doesn't link raylib). Call doctest directly — NOT main(),
// because MinGW CRT's main() wrapper already calls WinMain (infinite loop).
#ifdef _WIN32
extern "C" int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;
    int argc = 1;
    static char* argv[] = {const_cast<char*>("test_constants")};
    doctest::Context ctx(argc, argv);
    return ctx.run();
}
#endif

// Windows API function names conflict with raylib.h declarations.
// Since this test only checks compile-time constants (never calls any
// raylib functions), we rename raylib's symbols to avoid conflicts:
//   CloseWindow(HWND) vs CloseWindow(void)
//   ShowCursor(BOOL)  vs ShowCursor(void)
//   LoadImage, DrawText, DrawTextEx are macros in winuser.h
#ifdef _WIN32
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#define CloseWindow raylib_CloseWindow
#define ShowCursor raylib_ShowCursor
#define LoadImage raylib_LoadImage
#define DrawText raylib_DrawText
#define DrawTextEx raylib_DrawTextEx
#endif

// Include hanya header yang mendefinisikan constants
#include "rendering/animation.h"       // FRAME_SIZE, FRAME_GAP, MAX_TEXTURES, State, Direction

// Restore original names (test code doesn't use any, but good practice)
#ifdef _WIN32
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#endif
#include "core/screen.h"               // Time::DELTA_TIME, Time::MAX_FRAME, ScreenState
#include "map/map.h"                   // COLLISION_LAYER_NAME, SPAWN_OBJECT_NAME, dll
#include "map/worldgenenartion.h"      // WG_*, CellType, ExitDirection
#include "entities/enemy.h"            // SPAWN_PINPOINT_*, EnemyAIState, EnemyRank, RayCastMode
#include "items/item.h"                // ItemCategory, ItemRarity, SpawnAreaSize, AttackType, InventoryItem
#include "map/propsbehavior.h"         // SpikeManager, BombManager, CrateManager, BarrierManager
#include "entities/enemy_ai.h"         // FLOW_FIELD_*, SEPARATION_*, CELL_SIZE
#include "core/seedmanager.h"          // SeedManager::SEED_COUNT
#include "core/game_state_saver.h"     // SAVE_VERSION

/*==============================================================================
 * Tile / Screen Constants
 *==============================================================================*/

TEST_CASE("FRAME_SIZE == 32")
{
    CHECK(FRAME_SIZE == 32);
}

TEST_CASE("FRAME_GAP == 4")
{
    CHECK(FRAME_GAP == 4);
}

TEST_CASE("MAX_TEXTURES == 7")
{
    CHECK(MAX_TEXTURES == 7);
}

TEST_CASE("Time::DELTA_TIME == 1.0f/60.0f")
{
    CHECK(Time::DELTA_TIME == 1.0f / 60.0f);
}

TEST_CASE("Time::MAX_FRAME == 0.25f")
{
    CHECK(Time::MAX_FRAME == 0.25f);
}

/*==============================================================================
 * Animation Enums
 *==============================================================================*/

TEST_CASE("State enum values")
{
    CHECK(State::IDLE == 0);
    CHECK(State::WALK == 1);
    CHECK(State::ATTACK == 2);
    CHECK(State::DEAD == 3);
}

TEST_CASE("Direction enum values")
{
    CHECK(Direction::LEFT == 0);
    CHECK(Direction::RIGHT == 1);
    CHECK(Direction::DOWN == 2);
    CHECK(Direction::UP == 3);
}

/*==============================================================================
 * ScreenState Enum
 *==============================================================================*/

TEST_CASE("ScreenState enum values")
{
    CHECK(MAIN_MENU == 0);
    CHECK(LOADING == 1);
    CHECK(PLAY == 2);
    CHECK(OPTIONS == 3);
    CHECK(GAME_OVER == 4);
}

/*==============================================================================
 * Map String Constants
 *==============================================================================*/

TEST_CASE("Map layer and object name constants")
{
    CHECK(std::string(COLLISION_LAYER_NAME) == "obstacle");
    CHECK(std::string(OBJECT_LAYER_NAME) == "object");
    CHECK(std::string(SPAWN_OBJECT_NAME) == "spawn");
    CHECK(std::string(DOOR_TYPE_OBJECT_NAME) == "pass");
    CHECK(std::string(CHEST_TYPE_OBJECT_NAME) == "chest");
    CHECK(std::string(SPIKE_TYPE_OBJECT_NAME) == "spike");
    CHECK(std::string(BOMB_TYPE_OBJECT_NAME) == "bomb");
    CHECK(std::string(CRATE_TYPE_OBJECT_NAME) == "crate");
    CHECK(std::string(SIGN_TYPE_OBJECT_NAME) == "sign");
    CHECK(std::string(BARRIER_TYPE_OBJECT_NAME) == "barrier");
    CHECK(std::string(BARRIER_BOSS_TYPE_OBJECT_NAME) == "barrier_boss");
    CHECK(std::string(BOSS_STAGE_TYPE_OBJECT_NAME) == "boss_stage");
    CHECK(std::string(TRAP_LAYER_NAME) == "trap");
    CHECK(std::string(ITEM_LAYER_NAME) == "item");
    CHECK(std::string(EXIT_LAYER_NAME) == "exit");
}

/*==============================================================================
 * Enemy Spawn Constants (namespace-scope constexpr)
 *==============================================================================*/

TEST_CASE("SPAWN_PINPOINT constants")
{
    CHECK(SPAWN_PINPOINT_NORMAL_MIN == 9);
    CHECK(SPAWN_PINPOINT_NORMAL_MAX == 13);
    CHECK(SPAWN_PINPOINT_ELITE_MIN == 3);
    CHECK(SPAWN_PINPOINT_ELITE_MAX == 7);
}

TEST_CASE("SPAWN_RECT constants")
{
    CHECK(SPAWN_RECT_NORMAL_MIN == 20);
    CHECK(SPAWN_RECT_NORMAL_MAX == 25);
    CHECK(SPAWN_RECT_ELITE_MIN == 10);
    CHECK(SPAWN_RECT_ELITE_MAX == 15);
}

TEST_CASE("SPAWN_RETRY_LIMIT == 200")
{
    CHECK(SPAWN_RETRY_LIMIT == 200);
}

/*==============================================================================
 * Enemy Enums
 *==============================================================================*/

TEST_CASE("EnemyAIState enum values")
{
    CHECK(ENEMY_IDLE == 0);
    CHECK(ENEMY_PATROL == 1);
    CHECK(ENEMY_CHASE == 2);
    CHECK(ENEMY_ATTACK == 3);
    CHECK(ENEMY_RETURN == 4);
}

TEST_CASE("RayCastMode enum values")
{
    CHECK(RayCastMode::LINE == 0);
    CHECK(RayCastMode::CONE == 1);
}

TEST_CASE("EnemyRank enum values")
{
    CHECK(ENEMY_NORMAL == 0);
    CHECK(ENEMY_ELITE == 1);
    CHECK(ENEMY_BOSS == 2);
}

/*==============================================================================
 * Worldgen Constants
 *==============================================================================*/

TEST_CASE("WG grid size constants")
{
    CHECK(WG_GRID_SIZE == 4);
    CHECK(WG_CELL_TILES == 41);
    CHECK(WG_CANVAS_TILES == 164);
}

TEST_CASE("WG layer constants")
{
    CHECK(WG_PREFAB_LAYER_START == 3);
    CHECK(WG_CORRIDOR_LAYER_START == 5);
}

TEST_CASE("WG tile size == FRAME_SIZE")
{
    CHECK(WG_TILE_SIZE == FRAME_SIZE);
    CHECK(WG_TILE_SIZE == 32);
}

TEST_CASE("WG Prim's algorithm constants")
{
    CHECK(WG_PRIM_START_CELLS == 1);
    CHECK(WG_PRIM_MIN_CELLS == 6);
    CHECK(WG_PRIM_MAX_CELLS == 10);
    CHECK(WG_PRIM_STOP_CHANCE == 20);
    CHECK(WG_PRIM_RETRY_MAX == 10);
}

TEST_CASE("WG variety iterations == 2")
{
    CHECK(WG_VARIETY_ITERATIONS == 2);
}

TEST_CASE("WG dir/sentinel constants")
{
    CHECK(NUM_DIRS == 4);
    CHECK(DEPTH_UNVISITED == -1);
    CHECK(INVALID_INDEX == -1);
    CHECK(PERCENT_MAX == 100);
}

TEST_CASE("WG corridor constants")
{
    CHECK(CORRIDOR_CENTER_OFFSET == 3);
    CHECK(CORRIDOR_LAYER_COUNT == 2);
}

/*==============================================================================
 * Worldgen String Constants
 *==============================================================================*/

TEST_CASE("Worldgen string constants")
{
    CHECK(std::string(SLOT_WORLDGEN_LAYER_NAME) == "slot_worldgen");
    CHECK(std::string(EXIT_NORTH_TYPE_OBJECT_NAME) == "exit_north");
    CHECK(std::string(EXIT_EAST_TYPE_OBJECT_NAME) == "exit_east");
    CHECK(std::string(EXIT_SOUTH_TYPE_OBJECT_NAME) == "exit_south");
    CHECK(std::string(EXIT_WEST_TYPE_OBJECT_NAME) == "exit_west");
}

/*==============================================================================
 * Worldgen CellType Enum
 *==============================================================================*/

TEST_CASE("CellType enum values")
{
    CHECK(CELL_EMPTY == 0);
    CHECK(CELL_START == 1);
    CHECK(CELL_ENEMY == 2);
    CHECK(CELL_ENEMY_ELITE == 3);
    CHECK(CELL_TREASURE == 4);
    CHECK(CELL_TRADER == 5);
    CHECK(CELL_FINISH == 6);
    CHECK(CELL_BOSS == 7);
    CHECK(CELL_SPECIAL == 8);
}

/*==============================================================================
 * Worldgen ExitDirection Enum
 *==============================================================================*/

TEST_CASE("ExitDirection enum values")
{
    CHECK(EXIT_NONE == 0);
    CHECK(EXIT_NORTH == 1);
    CHECK(EXIT_EAST == 2);
    CHECK(EXIT_SOUTH == 4);
    CHECK(EXIT_WEST == 8);
}

/*==============================================================================
 * Item Enums
 *==============================================================================*/

TEST_CASE("ItemCategory enum values")
{
    CHECK(ITEM_WEAPON == 0);
    CHECK(ITEM_POTION == 1);
    CHECK(ITEM_POISON == 2);
    CHECK(ITEM_ARMOR == 3);
    CHECK(ITEM_NONE == 4);
    CHECK(ITEM_ANY == 5);
}

TEST_CASE("ItemRarity enum values")
{
    CHECK(RARITY_COMMON == 0);
    CHECK(RARITY_UNCOMMON == 1);
    CHECK(RARITY_RARE == 2);
    CHECK(RARITY_EPIC == 3);
}

TEST_CASE("SpawnAreaSize enum values")
{
    CHECK(SPAWN_SIZE_SMALL == 0);
    CHECK(SPAWN_SIZE_MEDIUM == 1);
    CHECK(SPAWN_SIZE_LARGE == 2);
    CHECK(SPAWN_SIZE_XLARGE == 3);
}

TEST_CASE("AttackType enum values")
{
    CHECK(ATTACK_SLASH == 0);
    CHECK(ATTACK_THRUST == 1);
    CHECK(ATTACK_PIERCE == 2);
    CHECK(ATTACK_SLAM == 3);
}

TEST_CASE("InventoryItem default values")
{
    InventoryItem item;
    CHECK(item.definitionId == -1);
    CHECK(item.amount == 0);
}

/*==============================================================================
 * Props Constants (static constexpr)
 *==============================================================================*/

TEST_CASE("SpikeManager constants")
{
    CHECK(SpikeManager::SPIKE_ACTIVE_MAX == 6.0f);
    CHECK(SpikeManager::SPIKE_ACTIVE_MIN == 3.0f);
    CHECK(SpikeManager::SPIKE_INACTIVE_MAX == 7.0f);
    CHECK(SpikeManager::SPIKE_INACTIVE_MIN == 4.0f);
    CHECK(SpikeManager::SPIKE_DAMAGE == 10.0f);
    CHECK(SpikeManager::SPIKE_DAMAGE_COOLDOWN == 1.0f);
}

TEST_CASE("BombManager constants")
{
    CHECK(BombManager::BOMB_EXPLOSION_RADIUS == 80.0f);
    CHECK(BombManager::BOMB_DAMAGE == 25.0f);
    CHECK(BombManager::BOMB_EXPLOSION_DURATION == 0.6f);
}

TEST_CASE("CrateManager::CRATE_LOOT_CHANCE == 0.1f")
{
    CHECK(CrateManager::CRATE_LOOT_CHANCE == 0.1f);
}

TEST_CASE("BarrierManager::KILL_THRESHOLD == 0.9f")
{
    CHECK(BarrierManager::KILL_THRESHOLD == 0.9f);
}

/*==============================================================================
 * Flow Field / AI Constants
 *==============================================================================*/

TEST_CASE("FLOW_FIELD constants")
{
    CHECK(FLOW_FIELD_TILE_SIZE == FRAME_SIZE);
    CHECK(FLOW_FIELD_TILE_SIZE == 32);
    CHECK(FLOW_FIELD_CENTER_OFFSET == 16.0f);
    CHECK(FLOW_FIELD_REBUILD_COOLDOWN == 0.3f);
    CHECK(FLOW_FIELD_PLAYER_RADIUS == 10);
    CHECK(FLOW_FIELD_RETURN_RADIUS == 18);
    CHECK(STEERING_GRID_RADIUS == 2);
}

TEST_CASE("Separation constants")
{
    CHECK(SEPARATION_RADIUS == 28.0f);
    CHECK(SEPARATION_STRENGTH == 25.0f);
    CHECK(MAX_SEPARATION_FORCE == 30.0f);
}

TEST_CASE("CELL_SIZE == 57 (int, 32 * 1.8f truncates)")
{
    CHECK(CELL_SIZE == 57);
    CHECK(CELL_SIZE == doctest::Approx(57.6f).epsilon(0.1f)); // 57.6f truncated to int
}

/*==============================================================================
 * Seed / Save Constants
 *==============================================================================*/

TEST_CASE("SeedManager::SEED_COUNT == 5")
{
    CHECK(SeedManager::SEED_COUNT == 5);
}

TEST_CASE("SAVE_VERSION == 2")
{
    CHECK(SAVE_VERSION == 2);
}
