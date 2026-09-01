#pragma once

#include "components.h"
#include "vec2.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace sim {

// Scratch storage is intentionally owned by one worker at a time. The
// topology in NavGrid is immutable after build, while this state is reused by
// repeated searches to avoid clearing the entire grid for every request.
struct PathScratch {
    std::vector<float> G;
    std::vector<int> Parent;
    std::vector<uint32_t> SeenGeneration;
    std::vector<uint32_t> ClosedGeneration;
    std::vector<std::pair<float, int>> Open;
    uint32_t Generation = 0;

    void resize(size_t cell_count);
    void begin_search();
    bool seen(int index) const;
    bool closed(int index) const;
};

struct NavGrid {
    float CellSize = 0.5f;
    int Width = 0;
    int Height = 0;
    float OriginX = 0.0f;
    float OriginY = 0.0f;
    std::vector<uint8_t> Blocked;

    std::vector<std::pair<Vec2, Vec2>> InflatedWalls;

    uint32_t Revision = 1;

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
    std::vector<Vec2> find_path(
        Vec2 start,
        Vec2 goal,
        PathScratch &scratch,
        uint32_t *expanded_nodes = nullptr
    ) const;
    bool line_clear(Vec2 a, Vec2 b) const;

  private:
    static float octile_dist(int x0, int y0, int x1, int y1);
    static bool seg_intersects_aabb(Vec2 a, Vec2 b, Vec2 bb_min, Vec2 bb_max);
    std::vector<Vec2> smooth_path(const std::vector<Vec2> &raw) const;
};

} // namespace sim
