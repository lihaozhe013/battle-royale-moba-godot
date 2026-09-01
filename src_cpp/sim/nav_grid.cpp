#include "nav_grid.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace sim {

void PathScratch::resize(size_t cell_count) {
    const bool size_changed = G.size() != cell_count;
    G.resize(cell_count);
    Parent.resize(cell_count);
    if (size_changed) {
        SeenGeneration.assign(cell_count, 0);
        ClosedGeneration.assign(cell_count, 0);
        Generation = 0;
    } else {
        SeenGeneration.resize(cell_count, 0);
        ClosedGeneration.resize(cell_count, 0);
    }
    Open.reserve(std::min<size_t>(cell_count, 4096));
}

void PathScratch::begin_search() {
    if (Generation == std::numeric_limits<uint32_t>::max()) {
        std::fill(SeenGeneration.begin(), SeenGeneration.end(), 0);
        std::fill(ClosedGeneration.begin(), ClosedGeneration.end(), 0);
        Generation = 1;
    } else {
        ++Generation;
        if (Generation == 0)
            Generation = 1;
    }
    Open.clear();
}

bool PathScratch::seen(int index) const {
    return SeenGeneration[index] == Generation;
}

bool PathScratch::closed(int index) const {
    return ClosedGeneration[index] == Generation;
}

void NavGrid::build(
    float map_half,
    const std::vector<WallBounds> &walls,
    float cell_size,
    float agent_radius
) {
    CellSize = cell_size;
    float half = map_half;
    OriginX = -half;
    OriginY = -half;
    Width = static_cast<int>(std::ceil(2.0f * half / cell_size));
    Height = Width;

    int total = Width * Height;
    Blocked.assign(total, 0);

    float padding = agent_radius + 0.25f;
    InflatedWalls.clear();
    for (auto &w : walls) {
        Vec2 min{w.Min.x - padding, w.Min.y - padding};
        Vec2 max{w.Max.x + padding, w.Max.y + padding};
        InflatedWalls.push_back({min, max});
    }

    for (int cy = 0; cy < Height; ++cy) {
        for (int cx = 0; cx < Width; ++cx) {
            int idx = cy * Width + cx;
            float cell_lx = OriginX + cx * CellSize;
            float cell_ly = OriginY + cy * CellSize;
            float cell_rx = cell_lx + CellSize;
            float cell_ry = cell_ly + CellSize;

            if (cell_lx < -half || cell_rx > half || cell_ly < -half ||
                cell_ry > half) {
                Blocked[idx] = 1;
                continue;
            }

            for (auto &[wmin, wmax] : InflatedWalls) {
                if (cell_rx > wmin.x && cell_lx < wmax.x && cell_ry > wmin.y &&
                    cell_ly < wmax.y) {
                    Blocked[idx] = 1;
                    break;
                }
            }
        }
    }

    ++Revision;
    if (Revision == 0)
        Revision = 1;
}

bool NavGrid::world_to_cell(Vec2 w, int &cx, int &cy) const {
    cx = static_cast<int>(std::floor((w.x - OriginX) / CellSize));
    cy = static_cast<int>(std::floor((w.y - OriginY) / CellSize));
    return cx >= 0 && cx < Width && cy >= 0 && cy < Height;
}

Vec2 NavGrid::cell_to_world(int cx, int cy) const {
    return Vec2{
        OriginX + (cx + 0.5f) * CellSize, OriginY + (cy + 0.5f) * CellSize
    };
}

bool NavGrid::is_blocked(int cx, int cy) const {
    if (cx < 0 || cx >= Width || cy < 0 || cy >= Height)
        return true;
    return Blocked[cy * Width + cx] != 0;
}

Vec2 NavGrid::snap_to_nearest_free(Vec2 w) const {
    int cx, cy;
    if (!world_to_cell(w, cx, cy))
        return w;
    if (!is_blocked(cx, cy))
        return cell_to_world(cx, cy);

    int max_r = std::max(Width, Height);
    for (int r = 1; r <= max_r; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dy = -r; dy <= r; ++dy) {
                if (std::abs(dx) != r && std::abs(dy) != r)
                    continue;
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx < Width && ny >= 0 && ny < Height &&
                    !is_blocked(nx, ny)) {
                    return cell_to_world(nx, ny);
                }
            }
        }
    }
    return w;
}

std::vector<Vec2> NavGrid::find_path(Vec2 start, Vec2 goal) const {
    PathScratch scratch;
    return find_path(start, goal, scratch);
}

std::vector<Vec2> NavGrid::find_path(
    Vec2 start,
    Vec2 goal,
    PathScratch &scratch,
    uint32_t *expanded_nodes
) const {
    if (expanded_nodes)
        *expanded_nodes = 0;
    int scx, scy, gcx, gcy;
    if (!world_to_cell(start, scx, scy) || !world_to_cell(goal, gcx, gcy))
        return {};

    if (is_blocked(scx, scy)) {
        Vec2 free = snap_to_nearest_free(start);
        world_to_cell(free, scx, scy);
    }
    if (is_blocked(gcx, gcy)) {
        Vec2 free = snap_to_nearest_free(goal);
        world_to_cell(free, gcx, gcy);
    }

    if (is_blocked(scx, scy) || is_blocked(gcx, gcy))
        return {};

    int start_idx = scy * Width + scx;
    int goal_idx = gcy * Width + gcx;

    if (start_idx == goal_idx)
        return {cell_to_world(scx, scy)};

    // Most movement requests do not cross an inflated wall. Preserve the
    // existing smoothed output shape while avoiding an open-list traversal
    // for that common case.
    const Vec2 snapped_goal = cell_to_world(gcx, gcy);
    if (line_clear(start, snapped_goal)) {
        if (expanded_nodes)
            *expanded_nodes = 1;
        return {start, snapped_goal};
    }

    const size_t total = static_cast<size_t>(Width) * Height;
    if (scratch.G.size() != total)
        scratch.resize(total);
    scratch.begin_search();
    const uint32_t generation = scratch.Generation;

    auto heuristic = [&](int idx) -> float {
        int cx = idx % Width;
        int cy = idx / Width;
        float dx = std::abs(static_cast<float>(cx - gcx));
        float dy = std::abs(static_cast<float>(cy - gcy));
        return (std::max(dx, dy) +
                (std::sqrt(2.0f) - 1.0f) * std::min(dx, dy)) *
               CellSize;
    };

    using PQItem = std::pair<float, int>;
    auto push_open = [&](PQItem item) {
        scratch.Open.push_back(item);
        std::push_heap(
            scratch.Open.begin(), scratch.Open.end(), std::greater<PQItem>()
        );
    };
    auto pop_open = [&]() -> PQItem {
        std::pop_heap(
            scratch.Open.begin(), scratch.Open.end(), std::greater<PQItem>()
        );
        PQItem item = scratch.Open.back();
        scratch.Open.pop_back();
        return item;
    };

    scratch.G[start_idx] = 0.0f;
    scratch.Parent[start_idx] = -1;
    scratch.SeenGeneration[start_idx] = generation;
    push_open({heuristic(start_idx), start_idx});

    const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };
    const float move_cost[8] = {
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        std::sqrt(2.0f),
        std::sqrt(2.0f),
        std::sqrt(2.0f),
        std::sqrt(2.0f)
    };

    while (!scratch.Open.empty()) {
        auto [f, idx] = pop_open();
        (void)f;

        if (scratch.closed(idx))
            continue;
        scratch.ClosedGeneration[idx] = generation;
        if (expanded_nodes)
            ++(*expanded_nodes);

        if (idx == goal_idx)
            break;

        int cx = idx % Width;
        int cy = idx / Width;

        for (int d = 0; d < 8; ++d) {
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];

            if (nx < 0 || nx >= Width || ny < 0 || ny >= Height)
                continue;
            if (is_blocked(nx, ny))
                continue;

            int nidx = ny * Width + nx;
            if (scratch.closed(nidx))
                continue;

            float current_g = scratch.seen(nidx)
                                  ? scratch.G[nidx]
                                  : std::numeric_limits<float>::max();
            float nd = scratch.G[idx] + move_cost[d] * CellSize;
            if (nd < current_g) {
                scratch.G[nidx] = nd;
                scratch.Parent[nidx] = idx;
                scratch.SeenGeneration[nidx] = generation;
                push_open({nd + heuristic(nidx), nidx});
            }
        }
    }

    if (!scratch.closed(goal_idx))
        return {};

    std::vector<int> cell_path;
    int cur = goal_idx;
    while (cur != -1) {
        cell_path.push_back(cur);
        cur = scratch.Parent[cur];
    }
    std::reverse(cell_path.begin(), cell_path.end());

    std::vector<Vec2> raw;
    raw.reserve(cell_path.size());
    for (int idx : cell_path) {
        int cx = idx % Width;
        int cy = idx / Width;
        raw.push_back(cell_to_world(cx, cy));
    }

    auto result = smooth_path(raw);
    if (!result.empty()) {
        result[0] = start;
    }
    return result;
}

float NavGrid::octile_dist(int x0, int y0, int x1, int y1) {
    float dx = std::abs(static_cast<float>(x0 - x1));
    float dy = std::abs(static_cast<float>(y0 - y1));
    return std::max(dx, dy) + (std::sqrt(2.0f) - 1.0f) * std::min(dx, dy);
}

bool NavGrid::seg_intersects_aabb(Vec2 a, Vec2 b, Vec2 bb_min, Vec2 bb_max) {
    float tmin = 0.0f, tmax = 1.0f;
    for (int dim = 0; dim < 2; ++dim) {
        float d = (dim == 0) ? (b.x - a.x) : (b.y - a.y);
        float a_v = (dim == 0) ? a.x : a.y;
        float min_v = (dim == 0) ? bb_min.x : bb_min.y;
        float max_v = (dim == 0) ? bb_max.x : bb_max.y;

        if (std::abs(d) < 1e-8f) {
            if (a_v < min_v || a_v > max_v)
                return false;
            continue;
        }

        float t1 = (min_v - a_v) / d;
        float t2 = (max_v - a_v) / d;
        if (t1 > t2)
            std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax)
            return false;
    }
    return true;
}

bool NavGrid::line_clear(Vec2 a, Vec2 b) const {
    for (auto &[wmin, wmax] : InflatedWalls) {
        if (seg_intersects_aabb(a, b, wmin, wmax))
            return false;
    }
    return true;
}

std::vector<Vec2> NavGrid::smooth_path(const std::vector<Vec2> &raw) const {
    if (raw.size() < 3)
        return raw;

    std::vector<Vec2> result;
    result.push_back(raw[0]);

    int last = 0;
    for (int i = 2; i < static_cast<int>(raw.size()); ++i) {
        if (!line_clear(raw[last], raw[i])) {
            result.push_back(raw[i - 1]);
            last = i - 1;
        }
    }
    if (last < static_cast<int>(raw.size()) - 1)
        result.push_back(raw.back());

    return result;
}

} // namespace sim
