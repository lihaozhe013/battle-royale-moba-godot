#pragma once

#include "../components.h"
#include "../game_config.h"
#include <entt/entt.hpp>

namespace sim {

inline void arrow_movement_system(entt::registry &reg, float dt) {
    auto view = reg.view<ArrowTag, Position2D, Velocity2D, Lifetime>();
    for (auto e : view) {
        auto &pos = view.get<Position2D>(e);
        auto &vel = view.get<Velocity2D>(e);
        auto &life = view.get<Lifetime>(e);

        // Homing: 每 tick 修正速度朝目标当前位置
        if (reg.all_of<Homing>(e)) {
            auto &hom = reg.get<Homing>(e);
            if (reg.valid(hom.Target) &&
                !(reg.all_of<Dead>(hom.Target) && reg.get<Dead>(hom.Target).enabled)) {
                Vec2 target_pos = reg.get<Position2D>(hom.Target).Value;
                Vec2 to_target = target_pos - pos.Value;
                float dist = glm::length(to_target);
                if (dist > 0.001f) {
                    Vec2 dir = to_target / dist;
                    vel.Value = dir * GameConfig::ArrowSpeed;
                    if (reg.all_of<FacingAngle>(e))
                        reg.get<FacingAngle>(e).Radians = std::atan2(dir.y, dir.x);
                }
            }
        }

        pos.Value = pos.Value + vel.Value * dt;
        life.Remaining -= dt;
    }
}

} // namespace sim
