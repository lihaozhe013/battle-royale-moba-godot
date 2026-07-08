#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

constexpr float PM_PI = 3.14159265358979323846f;

inline float pm_angle_diff(float target, float current) {
    return std::remainder(target - current, 2.0f * PM_PI);
}

inline void
player_movement_system(entt::registry &reg, float dt, float map_half) {
    auto view = reg.view<
        PlayerTag,
        Position2D,
        FacingAngle,
        PlayerInputState,
        MoveSpeed>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &speed = view.get<MoveSpeed>(e);

        // Status gate (Root=禁锢=不移动, Stun=眩晕=不移动)
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Timer > 0.0f &&
                (st.Type == StatusType::Root || st.Type == StatusType::Stun))
                continue;
        }

        // Cast state gate (Aiming allows movement)
        bool cast_block = false;
        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            if (cs.State == CastState::Phase::Casting ||
                cs.State == CastState::Phase::Channeling ||
                cs.State == CastState::Phase::Dashing)
                cast_block = true;
        }

        // Stop command (S key in MOBA mode)
        if (input.Stop) {
            if (reg.all_of<MovePath>(e)) {
                auto &path = reg.get<MovePath>(e);
                path.Following = false;
            }
            // Don't move this tick; keep facing
            continue;
        }

        // WASD movement (highest priority when active)
        if (glm::length(input.Move) > 0.01f) {
            Vec2 dir = vec2_normalize(input.Move);
            angle.Radians = std::atan2(dir.y, dir.x);
            Vec2 step = dir * speed.Value * dt;
            pos.Value = vec2_clamp_to_map(pos.Value + step, map_half);
            // Cancel any active path when WASD is used
            if (reg.all_of<MovePath>(e)) {
                auto &path = reg.get<MovePath>(e);
                path.Following = false;
            }
            continue;
        }

        // Path-following movement (only when cast allows movement)
        if (!cast_block && reg.all_of<MovePath>(e)) {
            auto &path = reg.get<MovePath>(e);
            if (path.Following && path.CurrentIndex < static_cast<int>(path.Waypoints.size())) {
                auto smooth_facing = [&](Vec2 move_dir) {
                    float target = std::atan2(move_dir.y, move_dir.x);
                    float diff = pm_angle_diff(target, angle.Radians);
                    float max_turn = GameConfig::PathTurnRate * dt;
                    if (std::abs(diff) > max_turn)
                        diff = (diff > 0 ? max_turn : -max_turn);
                    angle.Radians += diff;
                };
                Vec2 target = path.Waypoints[path.CurrentIndex];
                Vec2 dir = target - pos.Value;
                float dist = vec2_length(dir);
                if (dist < 0.01f) {
                    path.CurrentIndex++;
                    if (path.CurrentIndex >= static_cast<int>(path.Waypoints.size())) {
                        path.Following = false;
                    } else {
                        Vec2 next_target = path.Waypoints[path.CurrentIndex];
                        Vec2 next_dir = vec2_normalize(next_target - pos.Value);
                        smooth_facing(next_dir);
                    }
                    continue;
                }
                dir = vec2_normalize(dir);
                smooth_facing(dir);
                Vec2 step = dir * speed.Value * dt;
                // Don't overshoot the waypoint
                float step_len = vec2_length(step);
                if (step_len >= dist) {
                    pos.Value = target;
                    path.CurrentIndex++;
                    if (path.CurrentIndex >= static_cast<int>(path.Waypoints.size())) {
                        path.Following = false;
                    } else {
                        Vec2 next_target = path.Waypoints[path.CurrentIndex];
                        Vec2 next_dir = vec2_normalize(next_target - pos.Value);
                        smooth_facing(next_dir);
                    }
                } else {
                    pos.Value = vec2_clamp_to_map(pos.Value + step, map_half);
                }
            }
        }
    }
}

} // namespace sim
