#pragma once

#include "components.h"
#include "vec2.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace sim {

struct NavGrid {
    float CellSize = 0.5f;
    int Width = 0;
    int Height = 0;
    float OriginX = 0.0f;
    float OriginY = 0.0f;
    std::vector<uint8_t> Blocked;

    std::vector<std::pair<Vec2, Vec2>> InflatedWalls;

    // A* scratch buffers (reused across find_path calls)
    mutable std::vector<float> G;
    mutable std::vector<float> F;
    mutable std::vector<int> Parent;
    mutable std::vector<bool> Closed;

    void build(float map_half, const std::vector<WallBounds> &walls,
               float cell_size, float agent_radius) {
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

                if (cell_lx < -half || cell_rx > half ||
                    cell_ly < -half || cell_ry > half) {
                    Blocked[idx] = 1;
                    continue;
                }

                for (auto &[wmin, wmax] : InflatedWalls) {
                    if (cell_rx > wmin.x && cell_lx < wmax.x &&
                        cell_ry > wmin.y && cell_ly < wmax.y) {
                        Blocked[idx] = 1;
                        break;
                    }
                }
            }
        }

        G.assign(total, std::numeric_limits<float>::max());
        F.assign(total, std::numeric_limits<float>::max());
        Parent.assign(total, -1);
        Closed.assign(total, false);
    }

    bool world_to_cell(Vec2 w, int &cx, int &cy) const {
        cx = static_cast<int>(std::floor((w.x - OriginX) / CellSize));
        cy = static_cast<int>(std::floor((w.y - OriginY) / CellSize));
        return cx >= 0 && cx < Width && cy >= 0 && cy < Height;
    }

    Vec2 cell_to_world(int cx, int cy) const {
        return Vec2{OriginX + (cx + 0.5f) * CellSize,
                    OriginY + (cy + 0.5f) * CellSize};
    }

    bool is_blocked(int cx, int cy) const {
        if (cx < 0 || cx >= Width || cy < 0 || cy >= Height)
            return true;
        return Blocked[cy * Width + cx] != 0;
    }

    Vec2 snap_to_nearest_free(Vec2 w) const {
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

    std::vector<Vec2> find_path(Vec2 start, Vec2 goal) const {
        int scx, scy, gcx, gcy;
        if (!world_to_cell(start, scx, scy) ||
            !world_to_cell(goal, gcx, gcy))
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

        std::fill(Closed.begin(), Closed.end(), false);
        std::fill(G.begin(), G.end(), std::numeric_limits<float>::max());
        std::fill(F.begin(), F.end(), std::numeric_limits<float>::max());
        std::fill(Parent.begin(), Parent.end(), -1);

        auto heuristic = [&](int idx) -> float {
            int cx = idx % Width;
            int cy = idx / Width;
            float dx = std::abs(static_cast<float>(cx - gcx));
            float dy = std::abs(static_cast<float>(cy - gcy));
            float h = std::max(dx, dy) + (std::sqrt(2.0f) - 1.0f) * std::min(dx, dy);
            return h * CellSize;
        };

        using PQItem = std::pair<float, int>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> open;

        G[start_idx] = 0.0f;
        F[start_idx] = heuristic(start_idx);
        open.push({F[start_idx], start_idx});

        const int dirs[8][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1},
            {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        const float move_cost[8] = {
            1.0f, 1.0f, 1.0f, 1.0f,
            std::sqrt(2.0f), std::sqrt(2.0f),
            std::sqrt(2.0f), std::sqrt(2.0f)};

        while (!open.empty()) {
            auto [f, idx] = open.top();
            open.pop();

            if (Closed[idx])
                continue;
            Closed[idx] = true;

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
                if (Closed[nidx])
                    continue;

                float nd = G[idx] + move_cost[d] * CellSize;
                if (nd < G[nidx]) {
                    G[nidx] = nd;
                    Parent[nidx] = idx;
                    F[nidx] = nd + heuristic(nidx);
                    open.push({F[nidx], nidx});
                }
            }
        }

        if (!Closed[goal_idx])
            return {};

        std::vector<int> cell_path;
        int cur = goal_idx;
        while (cur != -1) {
            cell_path.push_back(cur);
            cur = Parent[cur];
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

private:
    static float octile_dist(int x0, int y0, int x1, int y1) {
        float dx = std::abs(static_cast<float>(x0 - x1));
        float dy = std::abs(static_cast<float>(y0 - y1));
        return std::max(dx, dy) + (std::sqrt(2.0f) - 1.0f) * std::min(dx, dy);
    }

    static bool seg_intersects_aabb(Vec2 a, Vec2 b, Vec2 bb_min, Vec2 bb_max) {
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

    bool line_clear(Vec2 a, Vec2 b) const {
        for (auto &[wmin, wmax] : InflatedWalls) {
            if (seg_intersects_aabb(a, b, wmin, wmax))
                return false;
        }
        return true;
    }

    std::vector<Vec2> smooth_path(const std::vector<Vec2> &raw) const {
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
};

} // namespace sim
