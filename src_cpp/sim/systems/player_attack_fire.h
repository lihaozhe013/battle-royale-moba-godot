#pragma once

#include "../arrow_spawner.h"
#include "../components.h"
#include "../game_config.h"
#include <cstdio>
#include <entt/entt.hpp>

namespace sim {

inline void player_attack_fire_system(
    entt::registry &reg, double now, CommandBuffer &cb, IdState &ids
) {
    auto view = reg.view<
        PlayerTag, Position2D, CombatStats, NetworkId, AttackTarget>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;

        // Stun gate
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f) {
                printf("[ATK] skip stun\n");
                continue;
            }
        }

        // CastState gate — 施法中不普攻
        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            if (cs.State != CastState::Phase::None) {
                printf("[ATK] skip cast state=%d\n", (int)cs.State);
                continue;
            }
        }

        auto &at = view.get<AttackTarget>(e);
        if (at.Target == entt::null) {
            printf("[ATK] skip no target\n");
            continue;
        }
        if (!reg.valid(at.Target)) {
            printf("[ATK] skip invalid target\n");
            continue;
        }
        bool target_dead = reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled;
        if (target_dead) {
            printf("[ATK] skip target dead\n");
            continue;
        }

        auto &pos = view.get<Position2D>(e);
        auto &target_pos = reg.get<Position2D>(at.Target).Value;
        Vec2 delta = target_pos - pos.Value;
        float dist = glm::length(delta);
        if (dist > GameConfig::PlayerAttackRange) {
            printf("[ATK] skip out of range dist=%.2f range=%.2f\n", dist, GameConfig::PlayerAttackRange);
            continue;
        }

        float aim_angle = std::atan2(delta.y, delta.x);
        auto &stats = view.get<CombatStats>(e);
        auto &net = view.get<NetworkId>(e);

        ArrowSpawnContext ctx{
            cb, ids, now, pos.Value, aim_angle, net.Value, e, stats.Atk
        };
        ctx.homing_target = at.Target;
        ctx.homing_target_net_id = at.TargetNetworkId;

        bool fired = try_fire(stats, ctx);
        printf("[ATK] fire result=%d dist=%.2f\n", fired, dist);

        // 设置 facing angle 朝目标
        if (reg.all_of<FacingAngle>(e)) {
            reg.get<FacingAngle>(e).Radians = aim_angle;
        }
    }
}

} // namespace sim
