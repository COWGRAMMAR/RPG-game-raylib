/**
 * @file worldgenlogic.cpp
 * @brief Implementasi World Generation Logic System
 *
 * File ini berisi implementasi algoritma world generation:
 * - Prim's algorithm untuk layout dungeon (WorldGenLayout)
 * - Cell type assignment dan constraint enforcement
 * - Room prefab selection dan rotation (WorldGenCanvas)
 * - Worldgen grid lookup runtime
 */

#include "worldgenenartion.h"
#include <ctime>
#include <queue>

static uint64_t currentRunSeed = 0;

// Direction lookup: index 0-3 = UP, DOWN, LEFT, RIGHT
static const int DIR_DR[] = {-1, 1, 0, 0};
static const int DIR_DC[] = {0, 0, -1, 1};
static const int DIR_EXIT_MASK[] = {EXIT_NORTH, EXIT_SOUTH, EXIT_WEST, EXIT_EAST};
static const int DIR_OPPOSITE_MASK[] = {EXIT_SOUTH, EXIT_NORTH, EXIT_EAST, EXIT_WEST};

/** @brief Generate run seed baru berdasarkan waktu sistem */
uint64_t GenerateRunSeed(void)
{
    currentRunSeed = static_cast<uint64_t>(time(nullptr));
    return currentRunSeed;
}

/** @brief Dapatkan run seed yang aktif */
uint64_t GetCurrentRunSeed(void)
{
    return currentRunSeed;
}

// Init grid ke CELL_EMPTY semua
void WorldGenLayout::InitGrid()
{
    grid.assign(WG_GRID_SIZE, std::vector<WorldCell>(WG_GRID_SIZE, {CELL_EMPTY, EXIT_NONE}));
}

/** @brief Constructor — init grid dengan seed dan boss flag */
WorldGenLayout::WorldGenLayout(uint64_t seed, bool isBossStage)
    : wgRng(seed), isBoss(isBossStage)
{
    InitGrid();
}

/** @brief Dapatkan grid layout hasil generate */
const std::vector<std::vector<WorldCell>> &WorldGenLayout::GetGrid() const
{
    return grid;
}

// Rotasi exit mask sesuai degree
int WorldGenCanvas::RotateExitMask(int mask, int degrees)
{
    int steps = (degrees / 90) % 4;
    for (int i = 0; i < steps; i++)
    {
        int newMask = 0;
        if (mask & EXIT_NORTH)
            newMask |= EXIT_EAST;
        if (mask & EXIT_EAST)
            newMask |= EXIT_SOUTH;
        if (mask & EXIT_SOUTH)
            newMask |= EXIT_WEST;
        if (mask & EXIT_WEST)
            newMask |= EXIT_NORTH;
        mask = newMask;
    }
    return mask;
}

// Cek apakah cell hanya punya 1 exit (leaf node)
bool WorldGenLayout::IsLeaf(int r, int c) const
{
    int mask = grid[r][c].exitMask;
    int count = 0;
    if (mask & EXIT_NORTH)
        count++;
    if (mask & EXIT_SOUTH)
        count++;
    if (mask & EXIT_EAST)
        count++;
    if (mask & EXIT_WEST)
        count++;
    return count == 1;
}

// Cek apakah cell bertetangga dengan cell type tertentu
bool WorldGenLayout::IsAdjacentToType(int r, int c, CellType type) const
{
    for (int d = 0; d < NUM_DIRS; d++)
    {
        if (!(grid[r][c].exitMask & DIR_EXIT_MASK[d]))
            continue;
        int nr = r + DIR_DR[d];
        int nc = c + DIR_DC[d];

        if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
            continue;

        if (grid[nr][nc].type == type)
            return true;
    }
    return false;
}

// Hitung jumlah cell yang aktif (bukan EMPTY)
int WorldGenLayout::CountActiveCells() const
{
    int count = 0;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (grid[r][c].type != CELL_EMPTY)
                count++;
    return count;
}

// Hitung depth (jarak) tiap cell dari START via BFS
std::vector<std::vector<int>> WorldGenLayout::ComputeDepth() const
{
    std::vector<std::vector<int>> depth(WG_GRID_SIZE, std::vector<int>(WG_GRID_SIZE, DEPTH_UNVISITED));

    // Cari START
    int startR = INVALID_INDEX, startC = INVALID_INDEX;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (grid[r][c].type == CELL_START)
            {
                startR = r;
                startC = c;
            }

    if (startR == INVALID_INDEX)
        return depth;

    // DFS iteratif
    std::queue<std::pair<int, int>> q;
    q.push({startR, startC});
    depth[startR][startC] = 0;

    while (!q.empty())
    {
        auto [r, c] = q.front();
        q.pop();

        for (int d = 0; d < NUM_DIRS; d++)
        {
            if (!(grid[r][c].exitMask & DIR_EXIT_MASK[d]))
                continue;
            int nr = r + DIR_DR[d];
            int nc = c + DIR_DC[d];
            if (depth[nr][nc] != DEPTH_UNVISITED)
                continue;
            depth[nr][nc] = depth[r][c] + 1;
            q.push({nr, nc});
        }
    }

    return depth;
}

// Generate dungeon layout pake algoritma Prim's
void WorldGenLayout::RunPrims()
{
    // 1. Pilih random edge cell buat START
    // Edge cells: row 0, row 3, col 0, col 3
    std::vector<std::pair<int, int>> edgeCells;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (r == 0 || r == WG_GRID_SIZE - 1 || c == 0 || c == WG_GRID_SIZE - 1)
                edgeCells.push_back({r, c});

    std::uniform_int_distribution<int> dist(0, edgeCells.size() - 1);
    auto [startR, startC] = edgeCells[dist(wgRng)];
    grid[startR][startC].type = CELL_START;

    // 2. Prim's — frontier list
    std::vector<std::pair<int, int>> frontier;
    auto addFrontier = [&](int r, int c)
    {
        if (r < 0 || r >= WG_GRID_SIZE || c < 0 || c >= WG_GRID_SIZE)
            return;
        if (grid[r][c].type != CELL_EMPTY)
            return;
        if (std::find(frontier.begin(), frontier.end(), std::make_pair(r, c)) != frontier.end())
            return;
        frontier.push_back({r, c});
    };

    addFrontier(startR - 1, startC);
    addFrontier(startR + 1, startC);
    addFrontier(startR, startC - 1);
    addFrontier(startR, startC + 1);

    int activeCells = WG_PRIM_START_CELLS;

    while (!frontier.empty() && activeCells < WG_PRIM_MAX_CELLS)
    {
        // Pilih random dari frontier
        std::uniform_int_distribution<int> fd(0, frontier.size() - 1);
        int idx = fd(wgRng);
        auto [r, c] = frontier[idx];
        frontier.erase(frontier.begin() + idx);

        // Aktifkan cell
        grid[r][c].type = CELL_ENEMY;
        activeCells++;

        // Set exitMask dua arah ke semua neighbor aktif
        for (int d = 0; d < NUM_DIRS; d++)
        {
            int nr = r + DIR_DR[d];
            int nc = c + DIR_DC[d];
            if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
                continue;
            if (grid[nr][nc].type == CELL_EMPTY)
                continue;

            grid[r][c].exitMask |= DIR_EXIT_MASK[d]; // exit dari cell baru ke neighbor
            grid[nr][nc].exitMask |= DIR_OPPOSITE_MASK[d];
        }

        // Tambah neighbors ke frontier
        for (int d = 0; d < NUM_DIRS; d++)
            addFrontier(r + DIR_DR[d], c + DIR_DC[d]);

        // Random stop setelah WG_PRIM_MIN_CELLS
        if (activeCells >= WG_PRIM_MIN_CELLS)
        {
            std::uniform_int_distribution<int> stopRoll(0, PERCENT_MAX);
            if (stopRoll(wgRng) < WG_PRIM_STOP_CHANCE)
                break;
        }
    }

    // 3. Cari cell paling jauh dari start → CELL_FINISH
    auto depth = ComputeDepth();
    int maxDepth = INVALID_INDEX;
    int finishR = startR, finishC = startC;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            if (grid[r][c].type == CELL_EMPTY)
                continue;
            if (r == startR && c == startC)
                continue;
            if (depth[r][c] > maxDepth)
            {
                maxDepth = depth[r][c];
                finishR = r;
                finishC = c;
            }
        }
    grid[finishR][finishC].type = isBoss ? CELL_BOSS : CELL_FINISH;
}

// Assign type (TREASURE, TRADER, ELITE, SPECIAL) ke leaf cells
void WorldGenLayout::AssignCellTypes()
{
    auto depth = ComputeDepth();
    int total = CountActiveCells();

    // Kumpulin leaf candidates (bukan START/FINISH/BOSS, gak adjacent ke START/FINISH/BOSS)
    std::vector<std::pair<int, int>> leaves;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            if (grid[r][c].type == CELL_EMPTY)
                continue;
            if (grid[r][c].type == CELL_START ||
                grid[r][c].type == CELL_FINISH ||
                grid[r][c].type == CELL_BOSS)
                continue;
            if (IsAdjacentToType(r, c, CELL_START) ||
                IsAdjacentToType(r, c, CELL_FINISH) ||
                IsAdjacentToType(r, c, CELL_BOSS))
                continue;
            if (IsLeaf(r, c))
                leaves.push_back({r, c});
        }

    // Fallback 1: semua non-START/FINISH/BOSS yang memenuhi constraint
    if (leaves.empty())
    {
        for (int r = 0; r < WG_GRID_SIZE; r++)
            for (int c = 0; c < WG_GRID_SIZE; c++)
            {
                if (grid[r][c].type == CELL_EMPTY)
                    continue;
                if (grid[r][c].type == CELL_START ||
                    grid[r][c].type == CELL_FINISH ||
                    grid[r][c].type == CELL_BOSS)
                    continue;
                if (IsAdjacentToType(r, c, CELL_START) ||
                    IsAdjacentToType(r, c, CELL_FINISH) ||
                    IsAdjacentToType(r, c, CELL_BOSS))
                    continue;
                leaves.push_back({r, c});
            }
    }

    // Fallback 2: force assign by depth
    if (leaves.empty())
    {
        int maxDepth = INVALID_INDEX;
        std::pair<int, int> best = {INVALID_INDEX, INVALID_INDEX};
        for (int r = 0; r < WG_GRID_SIZE; r++)
            for (int c = 0; c < WG_GRID_SIZE; c++)
            {
                if (grid[r][c].type == CELL_EMPTY)
                    continue;
                if (grid[r][c].type == CELL_START ||
                    grid[r][c].type == CELL_FINISH ||
                    grid[r][c].type == CELL_BOSS)
                    continue;
                if (IsAdjacentToType(r, c, CELL_START) ||
                    IsAdjacentToType(r, c, CELL_FINISH) ||
                    IsAdjacentToType(r, c, CELL_BOSS))
                    continue;
                if (depth[r][c] > maxDepth)
                {
                    maxDepth = depth[r][c];
                    best = {r, c};
                }
            }
        if (best.first != INVALID_INDEX)
            leaves.push_back(best);
    }

    std::shuffle(leaves.begin(), leaves.end(), wgRng);
    std::uniform_int_distribution<int> chance(0, PERCENT_MAX);
    int leafIdx = 0;

    // 1. Treasure wajib 1
    if (leafIdx < (int)leaves.size())
    {
        auto [r, c] = leaves[leafIdx++];
        grid[r][c].type = CELL_TREASURE;
    }

    // 2. Treasure ke-2, chance 20% kalau total > 7
    int secondTreasureThreshold = 7;
    int secondTreasureChance = 20;
    if (total > secondTreasureThreshold && leafIdx < (int)leaves.size() && chance(wgRng) < secondTreasureChance)
    {
        auto [r, c] = leaves[leafIdx++];
        grid[r][c].type = CELL_TREASURE;
    }

    // 3. Trader, chance 30%
    int traderChance = 30;
    if (leafIdx < (int)leaves.size() && chance(wgRng) < traderChance)
    {
        auto [r, c] = leaves[leafIdx++];
        grid[r][c].type = CELL_TRADER;
    }

    // 4. Elite, max 2, kalau total > 6
    int finishDepth = 0;
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (grid[r][c].type == CELL_FINISH)
                finishDepth = depth[r][c];

    int eliteThreshold = 6;
    int eliteCount = 0;
    int maxEliteCount = 2;
    if (total > eliteThreshold)
    {
        int eliteDepthHalf = finishDepth / 2;
        for (int i = leafIdx; i < (int)leaves.size() && eliteCount < maxEliteCount; i++)
        {
            auto [r, c] = leaves[i];
            if (depth[r][c] > eliteDepthHalf && depth[r][c] < finishDepth)
            {
                grid[r][c].type = CELL_ENEMY_ELITE;
                leaves.erase(leaves.begin() + i);
                i--;
                eliteCount++;
                leafIdx++;
            }
        }
    }

    // 5. Special, chance 20%
    int specialChance = 20;
    if (leafIdx < (int)leaves.size() && chance(wgRng) < specialChance)
    {
        auto [r, c] = leaves[leafIdx++];
        grid[r][c].type = CELL_SPECIAL;
    }
    TraceLog(LOG_INFO, "Total active: %d, Leaves found: %d", total, (int)leaves.size());
}

// Prune START/FINISH/BOSS — sisakan 1 exit aja
void WorldGenLayout::PruneSingleExitCells()
{
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            CellType type = grid[r][c].type;
            if (type != CELL_START && type != CELL_FINISH && type != CELL_BOSS)
                continue;
            if (type == CELL_EMPTY)
                continue;

            int &mask = grid[r][c].exitMask;

            // Kumpulin arah neighbour aktif dan neighbour non-EMPTY yang valid
            std::vector<int> activeDirs;
            std::vector<int> validDirs;
            for (int d = 0; d < NUM_DIRS; d++)
            {
                int nr = r + DIR_DR[d];
                int nc = c + DIR_DC[d];
                if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
                    continue;
                if (grid[nr][nc].type == CELL_EMPTY)
                    continue;
                validDirs.push_back(d);
                if (mask & DIR_EXIT_MASK[d])
                    activeDirs.push_back(d);
            }

            // Handle 0 exit — cari neighbour non-EMPTY dan tambah edge bidirectional
            if (activeDirs.empty() && !validDirs.empty())
            {
                std::uniform_int_distribution<int> dist(0, (int)validDirs.size() - 1);
                int newDir = validDirs[dist(wgRng)];
                mask |= DIR_EXIT_MASK[newDir];
                int nr = r + DIR_DR[newDir];
                int nc = c + DIR_DC[newDir];
                grid[nr][nc].exitMask |= DIR_OPPOSITE_MASK[newDir];
                activeDirs.push_back(newDir);
            }

            if (activeDirs.size() <= 1)
                continue;

            // Simpan daftar asli sebelum constraint filter (buat removal nanti)
            std::vector<int> origActiveDirs = activeDirs;

            // Constraint START: harus milih exit yang path-nya nyampe FINISH/BOSS
            if (type == CELL_START)
            {
                // Cari posisi FINISH/BOSS
                int finishR = INVALID_INDEX, finishC = INVALID_INDEX;
                for (int tr = 0; tr < WG_GRID_SIZE; tr++)
                    for (int tc = 0; tc < WG_GRID_SIZE; tc++)
                        if (grid[tr][tc].type == CELL_FINISH || grid[tr][tc].type == CELL_BOSS)
                        {
                            finishR = tr;
                            finishC = tc;
                        }

                if (finishR != INVALID_INDEX)
                {
                    std::vector<int> goodDirs;
                    for (int d : origActiveDirs)
                    {
                        int origMask = mask;
                        for (int d2 : origActiveDirs)
                            if (d2 != d)
                                mask &= ~DIR_EXIT_MASK[d2];

                        auto testDepth = ComputeDepth();
                        if (testDepth[finishR][finishC] >= 0)
                            goodDirs.push_back(d);

                        mask = origMask;
                    }

                    if (!goodDirs.empty())
                        activeDirs = goodDirs;
                }
            }

            // Pilih 1 random untuk dipertahankan
            std::uniform_int_distribution<int> dist(0, (int)activeDirs.size() - 1);
            int keepDir = activeDirs[dist(wgRng)];

            // Hapus semua direction dari origActiveDirs kecuali keepDir
            for (int d : origActiveDirs)
            {
                if (d == keepDir)
                    continue;

                mask &= ~DIR_EXIT_MASK[d];

                int nr = r + DIR_DR[d];
                int nc = c + DIR_DC[d];
                if (nr >= 0 && nr < WG_GRID_SIZE && nc >= 0 && nc < WG_GRID_SIZE)
                    grid[nr][nc].exitMask &= ~DIR_OPPOSITE_MASK[d];
            }
        }
    }
}

// Hapus cell yang gak reachable dari START
void WorldGenLayout::RemoveDisconnectedCells()
{
    // BFS dari START buat cari semua cell yang reachable
    bool visited[WG_GRID_SIZE][WG_GRID_SIZE] = {};
    std::queue<std::pair<int, int>> q;

    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (grid[r][c].type == CELL_START)
            {
                q.push({r, c});
                visited[r][c] = true;
            }

    if (q.empty())
        return;

    while (!q.empty())
    {
        auto [r, c] = q.front();
        q.pop();

        for (int d = 0; d < NUM_DIRS; d++)
        {
            if (!(grid[r][c].exitMask & DIR_EXIT_MASK[d]))
                continue;
            int nr = r + DIR_DR[d];
            int nc = c + DIR_DC[d];
            if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
                continue;
            if (visited[nr][nc])
                continue;
            if (grid[nr][nc].type == CELL_EMPTY)
                continue;
            visited[nr][nc] = true;
            q.push({nr, nc});
        }
    }

    // Hapus cell yang gak reachable & bersihin exitMask tetangganya
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            if (grid[r][c].type == CELL_EMPTY)
                continue;
            if (visited[r][c])
                continue;

            int mask = grid[r][c].exitMask;
            for (int d = 0; d < NUM_DIRS; d++)
            {
                if (mask & DIR_EXIT_MASK[d])
                {
                    int nr = r + DIR_DR[d];
                    int nc = c + DIR_DC[d];
                    if (nr >= 0 && nr < WG_GRID_SIZE && nc >= 0 && nc < WG_GRID_SIZE)
                        grid[nr][nc].exitMask &= ~DIR_OPPOSITE_MASK[d];
                }
            }

            grid[r][c].type = CELL_EMPTY;
            grid[r][c].exitMask = 0;
        }
    }
}

// Batasin exit count sesuai pool yang tersedia per cell type
void WorldGenLayout::ConstrainExitsByPool()
{
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            CellType type = grid[r][c].type;
            if (type == CELL_EMPTY)
                continue;
            // ENEMY + SPECIAL: semua pool tersedia (u, ud, ur, udr, udrl)
            if (type == CELL_ENEMY || type == CELL_SPECIAL)
                continue;

            int &mask = grid[r][c].exitMask;
            int exitCount = 0;
            if (mask & EXIT_NORTH)
                exitCount++;
            if (mask & EXIT_SOUTH)
                exitCount++;
            if (mask & EXIT_EAST)
                exitCount++;
            if (mask & EXIT_WEST)
                exitCount++;

            if (exitCount <= 1)
                continue; // 'u' pool works for semua type

            bool needsPrune = false;

            // START/FINISH/BOSS/TRADER: cuma 'u' pool
            if (type == CELL_START || type == CELL_FINISH || type == CELL_BOSS || type == CELL_TRADER)
                needsPrune = true;

            // TREASURE/ELITE: 'u' dan 'ur' pool
            if (type == CELL_TREASURE || type == CELL_ENEMY_ELITE)
            {
                if (exitCount == 2)
                {
                    bool straight = ((mask & EXIT_NORTH) && (mask & EXIT_SOUTH)) ||
                                    ((mask & EXIT_EAST) && (mask & EXIT_WEST));
                    if (straight)
                        needsPrune = true;
                }
                else
                    needsPrune = true; // 3+ exit gak bisa match 'ur'
            }

            if (!needsPrune)
                continue;

            // Kumpulin direction valid yang masih nyambung ke neighbour non-EMPTY
            std::vector<int> validDirs;
            for (int d = 0; d < NUM_DIRS; d++)
            {
                if (!(mask & DIR_EXIT_MASK[d]))
                    continue;
                int nr = r + DIR_DR[d];
                int nc = c + DIR_DC[d];
                if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
                    continue;
                if (grid[nr][nc].type == CELL_EMPTY)
                    continue;
                validDirs.push_back(d);
            }

            if (validDirs.empty())
            {
                mask = 0;
                continue;
            }

            std::uniform_int_distribution<int> dist(0, (int)validDirs.size() - 1);
            int keepDir = validDirs[dist(wgRng)];

            // Hapus semua direction kecuali keepDir
            for (int d = 0; d < NUM_DIRS; d++)
            {
                if (d == keepDir)
                    continue;
                if (!(mask & DIR_EXIT_MASK[d]))
                    continue;
                mask &= ~DIR_EXIT_MASK[d];
                int nr = r + DIR_DR[d];
                int nc = c + DIR_DC[d];
                if (nr >= 0 && nr < WG_GRID_SIZE && nc >= 0 && nc < WG_GRID_SIZE)
                    grid[nr][nc].exitMask &= ~DIR_OPPOSITE_MASK[d];
            }
        }
    }
}

// Kurangi 1 exit dari cell dense (3+ exit) untuk variety
void WorldGenLayout::PruneOneDenseExit()
{
    int prefabExitCandidate = 3;

    // Kumpulin cell dense (exitCount >= 3), bukan START/FINISH/BOSS/EMPTY
    std::vector<std::pair<int, int>> candidates;
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            CellType type = grid[r][c].type;
            if (type == CELL_EMPTY)
                continue;
            if (type == CELL_START || type == CELL_FINISH || type == CELL_BOSS)
                continue;

            int exitCount = __builtin_popcount(grid[r][c].exitMask);
            if (exitCount == prefabExitCandidate)
                candidates.push_back({r, c});
        }
    }

    if (candidates.empty())
        return;

    std::uniform_int_distribution<int> cellDist(0, (int)candidates.size() - 1);
    auto [r, c] = candidates[cellDist(wgRng)];

    int &mask = grid[r][c].exitMask;

    // Kumpulin direction aktif
    std::vector<int> dirs;
    for (int d = 0; d < NUM_DIRS; d++)
        if (mask & DIR_EXIT_MASK[d])
            dirs.push_back(d);

    // Filter: jangan prune direction yang neighbour-nya cuma 1 exit (leaf)
    std::vector<int> safeDirs;
    for (int d : dirs)
    {
        int nr = r + DIR_DR[d];
        int nc = c + DIR_DC[d];
        if (nr < 0 || nr >= WG_GRID_SIZE || nc < 0 || nc >= WG_GRID_SIZE)
            continue;
        int nCount = __builtin_popcount(grid[nr][nc].exitMask);
        if (nCount > 1)
            safeDirs.push_back(d);
    }

    if (safeDirs.empty())
        return;

    // Acak urutan direction biar gak bias
    std::shuffle(safeDirs.begin(), safeDirs.end(), wgRng);

    // Verification: prune → cek apa FINISH masih reachable → restore kalau putus
    for (int removeDir : safeDirs)
    {
        int nr = r + DIR_DR[removeDir];
        int nc = c + DIR_DC[removeDir];
        int origMask = mask;
        int origNeighborMask = grid[nr][nc].exitMask;

        mask &= ~DIR_EXIT_MASK[removeDir];
        grid[nr][nc].exitMask &= ~DIR_OPPOSITE_MASK[removeDir];

        // Cek apakah FINISH/BOSS masih reachable dari START setelah prune
        auto depth = ComputeDepth();
        bool finishReachable = false;
        for (int dr = 0; dr < WG_GRID_SIZE && !finishReachable; dr++)
            for (int dc = 0; dc < WG_GRID_SIZE && !finishReachable; dc++)
                if ((grid[dr][dc].type == CELL_FINISH || grid[dr][dc].type == CELL_BOSS) &&
                    depth[dr][dc] != DEPTH_UNVISITED)
                    finishReachable = true;

        if (finishReachable)
            return; // Prune berhasil, keluar

        // Restore — prune ini putusin FINISH/BOSS, coba direction lain
        mask = origMask;
        grid[nr][nc].exitMask = origNeighborMask;
    }
}

// Pastikan minimal ada 1 CELL_TREASURE di grid
void WorldGenLayout::EnsureTreasureExists()
{
    // Cek apakah treasure masih ada
    for (int r = 0; r < WG_GRID_SIZE; r++)
        for (int c = 0; c < WG_GRID_SIZE; c++)
            if (grid[r][c].type == CELL_TREASURE)
                return;

    auto isBadNeighbor = [&](int r, int c) -> bool
    {
        return IsAdjacentToType(r, c, CELL_FINISH) ||
               IsAdjacentToType(r, c, CELL_BOSS) ||
               IsAdjacentToType(r, c, CELL_START);
    };

    // Type yang gak boleh di-replace: START, FINISH, BOSS
    auto isProtectedType = [](CellType t) -> bool
    {
        return t == CELL_START || t == CELL_FINISH || t == CELL_BOSS ||
               t == CELL_TREASURE; // TREASURE udah dicek di atas, safety aja
    };

    // Cari kandidat yang bentukannya cocok & gak adjacent ke FINISH/BOSS/START
    std::vector<std::pair<int, int>> candidates;
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            if (isProtectedType(grid[r][c].type))
                continue;
            if (isBadNeighbor(r, c))
                continue;

            int mask = grid[r][c].exitMask;
            int exitCount = 0;
            if (mask & EXIT_NORTH)
                exitCount++;
            if (mask & EXIT_SOUTH)
                exitCount++;
            if (mask & EXIT_EAST)
                exitCount++;
            if (mask & EXIT_WEST)
                exitCount++;

            if (exitCount == 1)
                candidates.push_back({r, c});
            else if (exitCount == 2)
            {
                bool straight = ((mask & EXIT_NORTH) && (mask & EXIT_SOUTH)) ||
                                ((mask & EXIT_EAST) && (mask & EXIT_WEST));
                if (!straight)
                    candidates.push_back({r, c});
            }
        }
    }

    if (candidates.empty())
    {
        // Fallback 1: replace type apa pun (kecuali protected) yang gak adjacent
        for (int r = 0; r < WG_GRID_SIZE; r++)
            for (int c = 0; c < WG_GRID_SIZE; c++)
                if (!isProtectedType(grid[r][c].type) && !isBadNeighbor(r, c))
                    candidates.push_back({r, c});
    }

    if (candidates.empty())
    {
        // Fallback 2: replace apa pun (kecuali protected, adjacent boleh)
        for (int r = 0; r < WG_GRID_SIZE; r++)
            for (int c = 0; c < WG_GRID_SIZE; c++)
                if (!isProtectedType(grid[r][c].type))
                    candidates.push_back({r, c});
    }

    if (candidates.empty())
        return;

    std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
    auto [r, c] = candidates[dist(wgRng)];
    grid[r][c].type = CELL_TREASURE;
}

// Print grid ke log untuk debugging
void WorldGenLayout::DebugPrintGrid() const
{
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        std::string row = "";
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            switch (grid[r][c].type)
            {
            case CELL_EMPTY:
                row += "[  ] ";
                break;
            case CELL_START:
                row += "[ST] ";
                break;
            case CELL_ENEMY:
                row += "[EN] ";
                break;
            case CELL_ENEMY_ELITE:
                row += "[EL] ";
                break;
            case CELL_TREASURE:
                row += "[TR] ";
                break;
            case CELL_TRADER:
                row += "[TD] ";
                break;
            case CELL_FINISH:
                row += "[FN] ";
                break;
            case CELL_BOSS:
                row += "[BS] ";
                break;
            case CELL_SPECIAL:
                row += "[SP] ";
                break;
            }
        }
        TraceLog(LOG_INFO, "%s", row.c_str());
    }

    // Print exitMask per cell
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        std::string row = "";
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            int mask = grid[r][c].exitMask;
            std::string exits = "";
            if (mask & EXIT_NORTH)
                exits += "N";
            if (mask & EXIT_SOUTH)
                exits += "S";
            if (mask & EXIT_EAST)
                exits += "E";
            if (mask & EXIT_WEST)
                exits += "W";
            if (exits.empty())
                exits = "--";
            row += "[" + exits + "] ";
        }
        TraceLog(LOG_INFO, "%s", row.c_str());
    }
}

/** @brief Constructor — wrapping existing TilesonMapData */
WorldGenPrefab::WorldGenPrefab(TilesonMapData *existingData)
    : data(existingData)
{
}

// Pilih prefab room yang cocok dengan type & exitMask, lalu rotate
WorldGenPrefab WorldGenCanvas::ResolveRotation(CellType type, int exitMask, std::mt19937_64 &rng)
{
    RoomPool &roomPool = pools->GetPoolForType(type);
    int baseMask = 0;
    WeightedPool *pool = SelectPool(type, exitMask, roomPool, baseMask);
    if (!pool || pool->prefabs.empty())
        return WorldGenPrefab();

    std::uniform_int_distribution<int> dist(0, (int)pool->prefabs.size() - 1);
    TilesonMapData *rawPrefab = pool->prefabs[dist(rng)];

    WorldGenPrefab wrapper(rawPrefab); // non-owning, pool masih punya data

    for (int deg : {0, 90, 180, 270})
    {
        if (RotateExitMask(baseMask, deg) == exitMask)
            return wrapper.Rotate(deg, deg); // owned copy hasil rotate
    }

    return wrapper.Rotate(0, 0); // fallback — owned copy tanpa rotasi
}

// Pilih pool yang sesuai berdasarkan cell type & exit count
WeightedPool *WorldGenCanvas::SelectPool(CellType type, int exitMask, RoomPool &roomPool, int &outBaseMask)
{
    int exitCount = __builtin_popcount(exitMask);

    if (type == CELL_START || type == CELL_FINISH || type == CELL_BOSS || type == CELL_TRADER)
    {
        outBaseMask = EXIT_NORTH;
        return &roomPool.u;
    }

    if (type == CELL_TREASURE || type == CELL_ENEMY_ELITE)
    {
        if (exitCount >= 2)
        {
            outBaseMask = EXIT_NORTH | EXIT_EAST;
            return &roomPool.ur;
        }
        else
        {
            outBaseMask = EXIT_NORTH;
            return &roomPool.u;
        }
    }

    // ENEMY + SPECIAL
    if (exitCount == 1)
    {
        outBaseMask = EXIT_NORTH;
        return &roomPool.u;
    }
    if (exitCount == 2)
    {
        bool straight = ((exitMask & EXIT_NORTH) && (exitMask & EXIT_SOUTH)) ||
                        ((exitMask & EXIT_EAST) && (exitMask & EXIT_WEST));
        if (straight)
        {
            outBaseMask = EXIT_NORTH | EXIT_SOUTH;
            return &roomPool.ud;
        }
        else
        {
            outBaseMask = EXIT_NORTH | EXIT_EAST;
            return &roomPool.ur;
        }
    }
    if (exitCount == 3)
    {
        outBaseMask = EXIT_NORTH | EXIT_SOUTH | EXIT_EAST;
        return &roomPool.udr;
    }
    outBaseMask = EXIT_NORTH | EXIT_SOUTH | EXIT_EAST | EXIT_WEST;
    return &roomPool.udrl;
}

/** @brief Generate dungeon layout dengan retry logic */
void WorldGenLayout::Generate()
{
    for (int attempt = 0; attempt < WG_PRIM_RETRY_MAX; attempt++)
    {
        InitGrid();
        RunPrims();
        AssignCellTypes();
        PruneSingleExitCells();
        TraceLog(LOG_INFO, "=== WORLDGEN: before filter ===");
        DebugPrintGrid();
        ConstrainExitsByPool();
        for (int i = 0; i < WG_VARIETY_ITERATIONS; i++)
            PruneOneDenseExit();
        RemoveDisconnectedCells();
        PruneSingleExitCells();
        EnsureTreasureExists();

        int count = CountActiveCells();
        TraceLog(LOG_INFO, "=== WORLDGEN: after filter (count=%d) ===", count);
        DebugPrintGrid();

        bool hasFinishOrBoss = false;
        for (int r = 0; r < WG_GRID_SIZE && !hasFinishOrBoss; r++)
            for (int c = 0; c < WG_GRID_SIZE && !hasFinishOrBoss; c++)
                if (grid[r][c].type == CELL_FINISH || grid[r][c].type == CELL_BOSS)
                    hasFinishOrBoss = true;

        if (count >= WG_PRIM_MIN_CELLS && hasFinishOrBoss)
            return;

        TraceLog(LOG_INFO, "=== WORLDGEN: retry %d/%d (cells=%d %s) ===",
                 attempt + 1, WG_PRIM_RETRY_MAX, count,
                 hasFinishOrBoss ? "" : ", no FINISH/BOSS cell");
        wgRng.seed(wgRng());
    }
}

/*==============================================================================
 * Worldgen Grid Lookup
 *==============================================================================*/

WorldgenCellInfo g_worldgenCells[WG_GRID_SIZE * WG_GRID_SIZE];
int g_worldgenCellCount = 0;

/** @brief Init worldgen grid lookup dari hasil generate */
void InitWorldgenGrid(const std::vector<std::vector<WorldCell>> &grid,
                      const std::vector<MapObject> &slots)
{
    g_worldgenCellCount = 0;
    for (int r = 0; r < WG_GRID_SIZE; r++)
    {
        for (int c = 0; c < WG_GRID_SIZE; c++)
        {
            int idx = r * WG_GRID_SIZE + c;
            g_worldgenCells[idx].type = grid[r][c].type;
            if (grid[r][c].type != CELL_EMPTY)
                g_worldgenCellCount++;
        }
    }

    // Map slot bounds ke cell yang sesuai. Slot naming: slot_1..slot_16
    // Tapi urutan di Tiled bisa acak, jadi mapping pake posisi grid
    for (auto &slot : slots)
    {
        int slotCol = (int)(slot.bounds.x / (WG_CELL_TILES * WG_TILE_SIZE));
        int slotRow = (int)(slot.bounds.y / (WG_CELL_TILES * WG_TILE_SIZE));
        if (slotCol >= 0 && slotCol < WG_GRID_SIZE &&
            slotRow >= 0 && slotRow < WG_GRID_SIZE)
        {
            int idx = slotRow * WG_GRID_SIZE + slotCol;
            g_worldgenCells[idx].bounds = slot.bounds;
        }
    }
}

/** @brief Cari index cell berdasarkan world position */
int GetCellIndexAtWorldPos(Vector2 worldPos)
{
    for (int i = 0; i < WG_GRID_SIZE * WG_GRID_SIZE; i++)
    {
        if (g_worldgenCells[i].type == CELL_EMPTY)
            continue;
        if (CheckCollisionPointRec(worldPos, g_worldgenCells[i].bounds))
            return i;
    }
    return -1;
}

/** @brief Dapatkan cell type di world position tertentu */
CellType GetCellTypeAtWorldPos(Vector2 worldPos)
{
    int idx = GetCellIndexAtWorldPos(worldPos);
    return (idx >= 0) ? g_worldgenCells[idx].type : CELL_EMPTY;
}

/** @brief Cek apakah world position memiliki cell type tertentu */
bool IsCellTypeAtWorldPos(Vector2 worldPos, CellType type)
{
    return GetCellTypeAtWorldPos(worldPos) == type;
}