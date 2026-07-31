#include "terrain.h"

#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace archipelago {
namespace Terrain {

namespace {

// Sampling resolution for the startup analysis (flood fill + site
// placement) — coarse enough to run in a fraction of a second, fine enough
// not to miss or badly mis-place anything at kBaseCellSize's scale.
constexpr int kGrid = 200;

struct Cell {
    int row;
    int col;
};

float CellWorldX(int col, float minX, float stepX) { return minX + stepX * static_cast<float>(col); }
float CellWorldZ(int row, float minZ, float stepZ) { return minZ + stepZ * static_cast<float>(row); }

float CellDistance(const Cell& a, const Cell& b, float stepX, float stepZ) {
    float dx = static_cast<float>(a.col - b.col) * stepX;
    float dz = static_cast<float>(a.row - b.row) * stepZ;
    return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

WorldLayout ComputeWorldLayout(uint32_t seed, float seaCenterX, float seaCenterZ, float seaHalfExtentX,
                                float seaHalfExtentZ) {
    float minX = seaCenterX - seaHalfExtentX;
    float minZ = seaCenterZ - seaHalfExtentZ;
    float stepX = (2.0f * seaHalfExtentX) / static_cast<float>(kGrid - 1);
    float stepZ = (2.0f * seaHalfExtentZ) / static_cast<float>(kGrid - 1);

    // Sampled in raw (unshifted) noise space — the offset that centers the
    // biggest island is derived from where it naturally lands, then applied
    // afterward to convert every site's position into shown/world space.
    std::vector<char> land(static_cast<size_t>(kGrid) * static_cast<size_t>(kGrid));
    for (int row = 0; row < kGrid; ++row) {
        float z = CellWorldZ(row, minZ, stepZ);
        for (int col = 0; col < kGrid; ++col) {
            float x = CellWorldX(col, minX, stepX);
            land[static_cast<size_t>(row) * static_cast<size_t>(kGrid) + static_cast<size_t>(col)] =
                Height(x, z, seed) > 0.0f ? 1 : 0;
        }
    }
    auto isLand = [&](int row, int col) -> bool {
        if (row < 0 || row >= kGrid || col < 0 || col >= kGrid) return false;
        return land[static_cast<size_t>(row) * static_cast<size_t>(kGrid) + static_cast<size_t>(col)] != 0;
    };

    // Flood fill every landmass, keep the cells of the biggest one.
    std::vector<char> visited(land.size(), 0);
    std::vector<Cell> bestCells;
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            size_t idx = static_cast<size_t>(row) * static_cast<size_t>(kGrid) + static_cast<size_t>(col);
            if (!land[idx] || visited[idx]) continue;

            std::vector<Cell> cells;
            std::queue<Cell> queue;
            queue.push({row, col});
            visited[idx] = 1;
            while (!queue.empty()) {
                Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);

                constexpr int kDeltaRow[4] = {-1, 1, 0, 0};
                constexpr int kDeltaCol[4] = {0, 0, -1, 1};
                for (int dir = 0; dir < 4; ++dir) {
                    int nr = cell.row + kDeltaRow[dir];
                    int nc = cell.col + kDeltaCol[dir];
                    if (nr < 0 || nr >= kGrid || nc < 0 || nc >= kGrid) continue;
                    size_t nidx = static_cast<size_t>(nr) * static_cast<size_t>(kGrid) + static_cast<size_t>(nc);
                    if (land[nidx] && !visited[nidx]) {
                        visited[nidx] = 1;
                        queue.push({nr, nc});
                    }
                }
            }

            if (cells.size() > bestCells.size()) bestCells = std::move(cells);
        }
    }

    WorldLayout layout;
    if (bestCells.empty()) return layout;  // no land found at all — fall back to zero offset, sites at sea center

    double sumX = 0.0, sumZ = 0.0;
    for (const Cell& cell : bestCells) {
        sumX += CellWorldX(cell.col, minX, stepX);
        sumZ += CellWorldZ(cell.row, minZ, stepZ);
    }
    float centroidX = static_cast<float>(sumX / static_cast<double>(bestCells.size()));
    float centroidZ = static_cast<float>(sumZ / static_cast<double>(bestCells.size()));
    layout.offset = CenterOffset{seaCenterX - centroidX, seaCenterZ - centroidZ};

    // Shoreline cells: land cells with at least one non-land (or off-grid)
    // neighbor — candidates for where a dock can actually reach the coast.
    std::vector<Cell> shoreline;
    for (const Cell& cell : bestCells) {
        if (!isLand(cell.row - 1, cell.col) || !isLand(cell.row + 1, cell.col) ||
            !isLand(cell.row, cell.col - 1) || !isLand(cell.row, cell.col + 1)) {
            shoreline.push_back(cell);
        }
    }
    if (shoreline.empty()) shoreline = bestCells;  // degenerate case (whole grid is this island) — fall back

    // Farthest-point sampling for 3 well-spread sites: start from the
    // centroid, then each next site maximizes its minimum distance to every
    // site already picked, so the 3 buildings end up spread around the
    // island instead of clustered on one stretch of coast.
    Cell centroidCell{static_cast<int>(std::round((centroidZ - minZ) / stepZ)),
                       static_cast<int>(std::round((centroidX - minX) / stepX))};
    auto farthestFrom = [&](const std::vector<Cell>& picked) -> Cell {
        Cell best = shoreline.front();
        float bestScore = -1.0f;
        for (const Cell& candidate : shoreline) {
            float minDist = std::numeric_limits<float>::max();
            for (const Cell& p : picked) {
                minDist = std::min(minDist, CellDistance(candidate, p, stepX, stepZ));
            }
            if (minDist > bestScore) {
                bestScore = minDist;
                best = candidate;
            }
        }
        return best;
    };

    Cell site1 = farthestFrom({centroidCell});
    Cell site2 = farthestFrom({site1});
    Cell site3 = farthestFrom({site1, site2});

    // Dock: nearest non-land cell to the chosen shoreline site, searched in
    // a small window around it (shoreline guarantees one exists close by).
    auto findDock = [&](const Cell& site) -> Vec3 {
        for (int radius = 1; radius <= 4; ++radius) {
            for (int dr = -radius; dr <= radius; ++dr) {
                for (int dc = -radius; dc <= radius; ++dc) {
                    int nr = site.row + dr;
                    int nc = site.col + dc;
                    if (nr < 0 || nr >= kGrid || nc < 0 || nc >= kGrid) continue;
                    if (!isLand(nr, nc)) {
                        return Vec3(CellWorldX(nc, minX, stepX) + layout.offset.x, 0.0f,
                                    CellWorldZ(nr, minZ, stepZ) + layout.offset.z);
                    }
                }
            }
        }
        // Shouldn't happen (site came from the shoreline list) — fall back
        // to the site's own position, offset into open water a bit.
        return Vec3(CellWorldX(site.col, minX, stepX) + layout.offset.x + stepX, 0.0f,
                    CellWorldZ(site.row, minZ, stepZ) + layout.offset.z);
    };

    auto toBuildingSite = [&](const Cell& site) -> BuildingSite {
        BuildingSite result;
        result.buildingPos =
            Vec3(CellWorldX(site.col, minX, stepX) + layout.offset.x, 0.0f, CellWorldZ(site.row, minZ, stepZ) + layout.offset.z);
        result.dockPos = findDock(site);
        return result;
    };

    layout.mine = toBuildingSite(site1);
    layout.mill = toBuildingSite(site2);
    layout.port = toBuildingSite(site3);
    return layout;
}

}  // namespace Terrain
}  // namespace archipelago
