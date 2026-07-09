#pragma once

#include "../arrow_spawner.h"
#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void bot_combat_system(
    entt::registry &reg, double now, CommandBuffer &cb, IdState &id_state
) {
    auto view = reg.view<
        BotTag,
        Position2D,
        FacingAngle,
        BotAIState,
        NetworkId,
        CombatStats>();
    for (auto e : view) {
        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        if (dead)
            continue;

        // Stun gate — 眩晕不能攻击
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f)
                continue;
        }

        auto &ai = view.get<BotAIState>(e);
        if (ai.TargetEntity == entt::null)
            continue;
        if (!reg.valid(ai.TargetEntity))
            continue;

        auto &stats = view.get<CombatStats>(e);
        auto &pos = view.get<Position2D>(e);
        auto &net = view.get<NetworkId>(e);

        // Compute shoot direction toward target (independent of FacingAngle,
        // which now follows movement direction — mirrors player_fire_system)
        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        if (glm::length(to_target) < 0.001f)
            continue;
        float target_angle = std::atan2(to_target.y, to_target.x);

        ArrowSpawnContext ctx{
            cb,
            id_state,
            now,
            pos.Value,
            target_angle,
            net.Value,
            e,
            stats.Atk};
        try_fire(stats, ctx);
    }
}

} // namespace sim
