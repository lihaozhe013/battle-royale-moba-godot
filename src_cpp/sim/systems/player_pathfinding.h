#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../nav_grid.h"
#include <cstdio>
#include <entt/entt.hpp>

namespace sim {

inline void player_pathfinding_system(entt::registry &reg, const NavGrid &nav) {
    auto view =
        reg.view<PlayerTag, Position2D, PlayerInputState, CastState, MovePath>(
        );
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &cs = view.get<CastState>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);

        // Any cast active → discard move command, stop following
        if (cs.State != CastState::Phase::None) {
            path.Following = false;
            continue;
        }

        // 1. 右键移动（A*）
        if (input.MoveIssue) {
            bool need_repath = true;
            if (path.Following) {
                Vec2 d = input.MoveTarget - path.FinalTarget;
                if (vec2_length_sq(d) < GameConfig::RepathTargetDeadzoneSq) {
                    need_repath = false;
                }
            }
            if (need_repath) {
                auto waypoints = nav.find_path(pos.Value, input.MoveTarget);
                if (!waypoints.empty()) {
                    path.Waypoints = std::move(waypoints);
                    path.CurrentIndex = 0;
                    path.FinalTarget = input.MoveTarget;
                    path.Following = true;
                }
            }
        }

        // 2. AttackTarget 追击（A* 绕墙）
        if (reg.all_of<AttackTarget>(e)) {
            auto &at = reg.get<AttackTarget>(e);
            if (at.Target != entt::null && reg.valid(at.Target)) {
                bool target_dead = reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled;
                if (!target_dead) {
                    Vec2 target_pos = reg.get<Position2D>(at.Target).Value;
                    Vec2 delta = target_pos - pos.Value;
                    float dist = glm::length(delta);

                    if (dist > GameConfig::PlayerAttackRange) {
                        // 超射程：用 A* 寻路追击（带死区防每 tick 重算）
                        bool need_chase_repath = true;
                        if (path.Following) {
                            Vec2 d = target_pos - path.FinalTarget;
                            if (vec2_length_sq(d) < GameConfig::RepathTargetDeadzoneSq) {
                                need_chase_repath = false;
                            }
                        }
                        if (need_chase_repath) {
                            auto waypoints = nav.find_path(pos.Value, target_pos);
                            if (!waypoints.empty()) {
                                path.Waypoints = std::move(waypoints);
                                path.CurrentIndex = 0;
                                path.FinalTarget = target_pos;
                                path.Following = true;
                                printf("[CHASE] A* repath to target=%d dist=%.2f\n", at.TargetNetworkId, dist);
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace sim
