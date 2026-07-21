#pragma once

#include "../arrow_spawner.h"
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

inline void attack_fire_system(
    entt::registry &reg, double now, CommandBuffer &cb, IdState &ids
) {
    auto view =
        reg.view<PlayerTag, Position2D, CombatStats, NetworkId, AttackTarget>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal && !reg.all_of<BotTag>(e))
            continue;

        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f)
                continue;
        }

        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            if (cs.State != CastState::Phase::None)
                continue;
        }

        auto &at = view.get<AttackTarget>(e);
        if (at.Target == entt::null)
            continue;
        if (!reg.valid(at.Target))
            continue;
        bool target_dead =
            reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled;
        if (target_dead)
            continue;

        auto &pos = view.get<Position2D>(e);
        auto &target_pos = reg.get<Position2D>(at.Target).Value;
        Vec2 delta = target_pos - pos.Value;
        float dist = glm::length(delta);
        if (dist > GameConfig::PlayerAttackRange)
            continue;

        float aim_angle = std::atan2(delta.y, delta.x);
        auto &stats = view.get<CombatStats>(e);
        auto &net = view.get<NetworkId>(e);

        ArrowSpawnContext ctx{
            cb, ids, now, pos.Value, aim_angle, net.Value, e, stats.Atk
        };
        ctx.homing_target = at.Target;
        ctx.homing_target_net_id = at.TargetNetworkId;

        try_fire(stats, ctx);

        if (reg.all_of<FacingAngle>(e)) {
            reg.get<FacingAngle>(e).Radians = aim_angle;
        }
    }
}

} // namespace sim
