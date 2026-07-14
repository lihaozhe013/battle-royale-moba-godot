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

        // ── 每帧重置 Chasing 标志 ──
        if (reg.all_of<AttackTarget>(e)) {
            reg.get<AttackTarget>(e).Chasing = false;
        }

        // ── Status gate (Root/Stun) ──
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Timer > 0.0f &&
                (st.Type == StatusType::Root || st.Type == StatusType::Stun))
                continue;
        }

        // ── Cast state gate ──
        // Casting/Channeling/Dashing 禁移动；Chasing/Aiming/None 允许
        bool cast_block = false;
        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            if (cs.State == CastState::Phase::Casting ||
                cs.State == CastState::Phase::Channeling ||
                cs.State == CastState::Phase::Dashing)
                cast_block = true;
        }

        // ── Stop ──
        if (input.Stop) {
            if (reg.all_of<MovePath>(e)) {
                reg.get<MovePath>(e).Following = false;
            }
            continue;
        }

        // ── 普攻直线追击（穿墙） ──
        // 条件：非 cast_block + 有 AttackTarget + 目标有效 + 超射程
        if (!cast_block && reg.all_of<AttackTarget>(e)) {
            auto &at = reg.get<AttackTarget>(e);
            if (at.Target != entt::null && reg.valid(at.Target)) {
                bool target_dead = reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled;
                if (!target_dead) {
                    Vec2 target_pos = reg.get<Position2D>(at.Target).Value;
                    Vec2 delta = target_pos - pos.Value;
                    float dist = glm::length(delta);

                    if (dist > GameConfig::PlayerAttackRange) {
                        // 直线朝目标移动（不走 A*）
                        Vec2 dir = delta / dist;

                        // 平滑朝向
                        float target_ang = std::atan2(dir.y, dir.x);
                        float diff = pm_angle_diff(target_ang, angle.Radians);
                        float max_turn = GameConfig::PathTurnRate * dt;
                        if (std::abs(diff) > max_turn)
                            diff = (diff > 0 ? max_turn : -max_turn);
                        angle.Radians += diff;

                        Vec2 step = dir * speed.Value * dt;
                        pos.Value = vec2_clamp_to_map(pos.Value + step, map_half);

                        // 设 Chasing 标志 → wall_collision 跳过此实体（穿墙）
                        at.Chasing = true;

                        // 清除 MovePath（追击优先于 A* 路径）
                        if (reg.all_of<MovePath>(e)) {
                            reg.get<MovePath>(e).Following = false;
                        }
                        continue;
                    }
                }
            }
        }

        // ── MovePath 跟随（右键移动 / 技能 Chasing A*） ──
        if (!cast_block && reg.all_of<MovePath>(e)) {
            auto &path = reg.get<MovePath>(e);
            if (path.Following &&
                path.CurrentIndex < static_cast<int>(path.Waypoints.size())) {

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
                    if (path.CurrentIndex >=
                        static_cast<int>(path.Waypoints.size())) {
                        path.Following = false;
                        continue;
                    }
                    target = path.Waypoints[path.CurrentIndex];
                    dir = target - pos.Value;
                    dist = vec2_length(dir);
                }

                dir = vec2_normalize(dir);
                smooth_facing(dir);

                Vec2 step = dir * speed.Value * dt;
                float step_len = vec2_length(step);

                if (step_len >= dist) {
                    pos.Value = target;
                    path.CurrentIndex++;
                    if (path.CurrentIndex >=
                        static_cast<int>(path.Waypoints.size())) {
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
