#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../nav_grid.h"
#include "../skills/skill_registry.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

inline void pathfinding_system(entt::registry &reg, const NavGrid &nav) {
    auto view = reg.view<
        PlayerTag,
        Position2D,
        PlayerInputState,
        CastState,
        MovePath>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        bool is_bot = reg.all_of<BotTag>(e);
        if (!tag.IsLocal && !is_bot)
            continue;

        auto &cs = view.get<CastState>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);

        if (cs.State == CastState::Phase::Chasing) {
            const ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
            if (!sk)
                continue;
            Vec2 chase_target;

            if (sk->kind() == SkillKind::MeleeSingle) {
                if (!reg.valid(cs.TargetEntity))
                    continue;
                chase_target = reg.get<Position2D>(cs.TargetEntity).Value;
            } else if (sk->kind() == SkillKind::AoEField) {
                chase_target = cs.AimPos;
            } else {
                continue;
            }

            bool need_repath = true;
            if (path.Following) {
                Vec2 d = chase_target - path.FinalTarget;
                if (vec2_length_sq(d) <
                    GameConfig::SkillChaseRepathDeadzoneSq) {
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
            continue;
        }

        if (cs.State == CastState::Phase::None) {
            if (reg.all_of<AttackTarget>(e)) {
                auto &at = reg.get<AttackTarget>(e);
                if (at.Target != entt::null && reg.valid(at.Target)) {
                    bool target_dead = reg.all_of<Dead>(at.Target) &&
                                       reg.get<Dead>(at.Target).enabled;
                    if (!target_dead) {
                        Vec2 target_pos = reg.get<Position2D>(at.Target).Value;
                        Vec2 delta = target_pos - pos.Value;
                        float dist = vec2_length_sq(delta);
                        if (dist > GameConfig::PlayerAttackRange *
                                       GameConfig::PlayerAttackRange) {
                            auto waypoints =
                                nav.find_path(pos.Value, target_pos);
                            if (!waypoints.empty()) {
                                path.Waypoints = std::move(waypoints);
                                path.CurrentIndex = 0;
                                path.FinalTarget = target_pos;
                                path.Following = true;
                            }
                        }
                    }
                    continue;
                }
            }
        }

        if (cs.State == CastState::Phase::Casting ||
            cs.State == CastState::Phase::Channeling ||
            cs.State == CastState::Phase::Dashing) {
            path.Following = false;
            continue;
        }

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
    }
}

} // namespace sim
