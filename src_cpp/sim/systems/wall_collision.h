#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "../command_buffer.h"

namespace sim {

inline void wall_collision_system(entt::registry &reg, CommandBuffer &cb) {
    auto wall_view = reg.view<WallTag, WallBounds>();
    if (wall_view.begin() == wall_view.end()) return;

    // Gather wall bounds
    std::vector<WallBounds> walls;
    for (auto w : wall_view) {
        walls.push_back(wall_view.get<WallBounds>(w));
    }

    // Movers (player/bot): push out of walls
    auto mover_view = reg.view<Damageable, Position2D>();
    for (auto e : mover_view) {
        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        if (dead) continue;

        float radius = reg.all_of<BotTag>(e) ? GameConfig::BotRadius : GameConfig::PlayerRadius;
        auto &pos = mover_view.get<Position2D>(e);

        Vec2 new_pos = pos.Value;
        for (auto &w : walls) {
            new_pos = resolve_circle_aabb(new_pos, radius, w.Min, w.Max);
        }
        pos.Value = new_pos;
    }

    // Arrows: destroy if inside any wall
    auto arrow_view = reg.view<ArrowTag, Position2D>();
    for (auto e : arrow_view) {
        auto &pos = arrow_view.get<Position2D>(e);
        for (auto &w : walls) {
            if (point_inside_aabb(pos.Value, w.Min, w.Max)) {
                cb.push([e](entt::registry &r) { r.destroy(e); });
                break;
            }
        }
    }
}

} // namespace sim
