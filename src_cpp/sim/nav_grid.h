#pragma once

#include "components.h"
#include "vec2.h"
#include <cstdint>
#include <limits>
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

    mutable std::vector<float> G;
    mutable std::vector<float> F;
    mutable std::vector<int> Parent;
    mutable std::vector<bool> Closed;

    void build(
        float map_half,
        const std::vector<WallBounds> &walls,
        float cell_size,
        float agent_radius
    );

    bool world_to_cell(Vec2 w, int &cx, int &cy) const;
    Vec2 cell_to_world(int cx, int cy) const;
    bool is_blocked(int cx, int cy) const;
    Vec2 snap_to_nearest_free(Vec2 w) const;
    std::vector<Vec2> find_path(Vec2 start, Vec2 goal) const;

  private:
    static float octile_dist(int x0, int y0, int x1, int y1);
    static bool seg_intersects_aabb(Vec2 a, Vec2 b, Vec2 bb_min, Vec2 bb_max);
    bool line_clear(Vec2 a, Vec2 b) const;
    std::vector<Vec2> smooth_path(const std::vector<Vec2> &raw) const;
};

} // namespace sim