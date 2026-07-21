#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include <entt/entt.hpp>

namespace sim {

inline entt::entity resolve_target_by_netid(entt::registry &reg, int net_id) {
    if (net_id < 0)
        return entt::null;
    auto view = reg.view<NetworkId, Damageable>();
    for (auto e : view) {
        if (view.get<NetworkId>(e).Value == net_id)
            return e;
    }
    return entt::null;
}

inline entt::entity find_nearest_enemy(
    entt::registry &reg, Vec2 pos, float max_radius, entt::entity exclude
) {
    entt::entity best = entt::null;
    float best_sq = max_radius * max_radius;
    auto view = reg.view<Damageable, Position2D>();
    for (auto e : view) {
        if (e == exclude)
            continue;
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
            continue;
        Vec2 delta = view.get<Position2D>(e).Value - pos;
        float dist_sq = vec2_length_sq(delta);
        if (dist_sq < best_sq) {
            best_sq = dist_sq;
            best = e;
        }
    }
    return best;
}

inline void attack_command_system(entt::registry &reg, float dt) {
    auto view =
        reg.view<PlayerTag, PlayerInputState, Position2D, AttackTarget>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        bool is_bot = reg.all_of<BotTag>(e);
        if (!tag.IsLocal && !is_bot)
            continue;
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
            continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &at = view.get<AttackTarget>(e);

        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f)
                continue;
        }

        if (input.Stop || (!is_bot && input.MoveIssue) || input.AttackClear) {
            at.Target = entt::null;
            at.TargetNetworkId = -1;
        }

        if (input.AttackTargetId >= 0) {
            entt::entity tgt =
                resolve_target_by_netid(reg, input.AttackTargetId);
            if (tgt != entt::null) {
                bool dead = reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled;
                if (!dead) {
                    bool new_target = (tgt != at.Target);
                    at.Target = tgt;
                    at.TargetNetworkId = input.AttackTargetId;
                    if (new_target && reg.all_of<CombatStats>(e))
                        reg.get<CombatStats>(e).LastFireTime = -999.0;
                }
            }
        }

        if (input.AttackGround) {
            entt::entity tgt = find_nearest_enemy(
                reg,
                input.AttackGroundPos,
                GameConfig::AttackAcquisitionRange,
                e
            );
            if (tgt != entt::null) {
                int net_id = reg.all_of<NetworkId>(tgt)
                                 ? reg.get<NetworkId>(tgt).Value
                                 : -1;
                bool new_target = (tgt != at.Target);
                at.Target = tgt;
                at.TargetNetworkId = net_id;
                if (new_target && reg.all_of<CombatStats>(e))
                    reg.get<CombatStats>(e).LastFireTime = -999.0;
            }
        }

        if (at.Target != entt::null) {
            if (!reg.valid(at.Target) || (reg.all_of<Dead>(at.Target) &&
                                          reg.get<Dead>(at.Target).enabled)) {
                at.Target = entt::null;
                at.TargetNetworkId = -1;
            }
        }
    }
}

} // namespace sim
