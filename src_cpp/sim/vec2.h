#pragma once

#include <cmath>
#include <glm/glm.hpp>

namespace sim {

using Vec2 = glm::vec2;

inline float vec2_length(Vec2 v) { return glm::length(v); }

inline float vec2_length_sq(Vec2 v) { return glm::dot(v, v); }

inline Vec2 vec2_normalize(Vec2 v) {
    float len = vec2_length(v);
    if (len < 1e-8f)
        return Vec2(0.0f);
    return v / len;
}

inline Vec2 vec2_clamp_to_map(Vec2 v, float half) {
    return Vec2(glm::clamp(v.x, -half, half), glm::clamp(v.y, -half, half));
}

inline bool circles_overlap(Vec2 a, float a_radius, Vec2 b, float b_radius) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float total_r = a_radius + b_radius;
    return dx * dx + dy * dy <= total_r * total_r;
}

inline bool point_inside_aabb(Vec2 p, Vec2 bb_min, Vec2 bb_max) {
    return p.x >= bb_min.x && p.x <= bb_max.x && p.y >= bb_min.y &&
           p.y <= bb_max.y;
}

inline Vec2
resolve_circle_aabb(Vec2 center, float radius, Vec2 bb_min, Vec2 bb_max) {
    Vec2 closest(
        glm::clamp(center.x, bb_min.x, bb_max.x),
        glm::clamp(center.y, bb_min.y, bb_max.y)
    );
    Vec2 diff = center - closest;
    float dist_sq = vec2_length_sq(diff);
    if (dist_sq < radius * radius && dist_sq > 1e-8f) {
        float dist = std::sqrt(dist_sq);
        float overlap = radius - dist;
        return center + (diff / dist) * overlap;
    }
    if (dist_sq < 1e-8f) {
        float to_left = center.x - bb_min.x;
        float to_right = bb_max.x - center.x;
        float to_bottom = center.y - bb_min.y;
        float to_top = bb_max.y - center.y;
        float min_dist = std::min({to_left, to_right, to_bottom, to_top});
        if (min_dist == to_left)
            return Vec2(bb_min.x - radius, center.y);
        if (min_dist == to_right)
            return Vec2(bb_max.x + radius, center.y);
        if (min_dist == to_bottom)
            return Vec2(center.x, bb_min.y - radius);
        if (min_dist == to_top)
            return Vec2(center.x, bb_max.y + radius);
    }
    return center;
}

} // namespace sim
