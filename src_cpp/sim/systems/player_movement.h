#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"

namespace sim {

inline void player_movement_system(entt::registry &reg, float dt, float map_half) {
    auto view = reg.view<PlayerTag, Position2D, FacingAngle, PlayerInputState, MoveSpeed>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &speed = view.get<MoveSpeed>(e);

        if (glm::length(input.Move) > 0.01f) {
            Vec2 dir = vec2_normalize(input.Move);
            angle.Radians = std::atan2(dir.y, dir.x);
            Vec2 step = dir * speed.Value * dt;
            pos.Value = vec2_clamp_to_map(pos.Value + step, map_half);
        }
    }
}

} // namespace sim
