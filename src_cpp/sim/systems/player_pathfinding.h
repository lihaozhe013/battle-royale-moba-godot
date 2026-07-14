#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../nav_grid.h"
#include "../skill_defs.h"
#include <cstdio>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

inline void player_pathfinding_system(entt::registry &reg, const NavGrid &nav) {
    auto view =
        reg.view<PlayerTag, Position2D, PlayerInputState, CastState, MovePath>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &cs = view.get<CastState>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);

        // ── 1. 技能 Chasing 阶段：A* 朝目标移动 ──
        if (cs.State == CastState::Phase::Chasing) {
            const auto &def = get_skill_def(cs.SkillId);
            Vec2 chase_target;

            if (def.Kind == SkillKind::MeleeSingle) {
                // 指向性技能：朝锁定目标寻路
                if (!reg.valid(cs.TargetEntity)) continue;
                chase_target = reg.get<Position2D>(cs.TargetEntity).Value;
            } else if (def.Kind == SkillKind::AoEField) {
                // 非指向性 AoE：朝鼠标落点寻路
                chase_target = cs.AimPos;
            } else {
                continue; // Dash/ChannelBurst 不会进 Chasing
            }

            bool need_repath = true;
            if (path.Following) {
                Vec2 d = chase_target - path.FinalTarget;
                if (vec2_length_sq(d) < GameConfig::SkillChaseRepathDeadzoneSq) {
                    need_repath = false;
                }
            }
            if (need_repath) {
                auto waypoints = nav.find_path(pos.Value, chase_target);
                if (!waypoints.empty()) {
                    path.Waypoints = std::move(waypoints);
                    path.CurrentIndex = 0;
                    path.FinalTarget = chase_target;
                    path.Following = true;
                }
            }
            // 不要 continue — 让 pathfinding 为 Chasing 写路径后，
            // 仍允许后续的 MoveIssue 处理（但 Chasing 状态会禁 MoveIssue 移动）
            continue;
        }

        // ── Casting/Channeling/Dashing → 停走 ──
        if (cs.State == CastState::Phase::Casting ||
            cs.State == CastState::Phase::Channeling ||
            cs.State == CastState::Phase::Dashing) {
            path.Following = false;
            continue;
        }

        // ── None/Aiming → 处理右键移动 ──
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
        // AttackTarget A* 追击已移除（普攻改直线穿墙，由 player_movement 处理）
    }
}

} // namespace sim
