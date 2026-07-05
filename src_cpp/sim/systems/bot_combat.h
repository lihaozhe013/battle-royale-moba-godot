#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../arrow_spawner.h"

namespace sim {

inline void bot_combat_system(entt::registry &reg, double now,
                              CommandBuffer &cb, IdState &id_state) {
    auto view = reg.view<BotTag, Position2D, FacingAngle, BotAIState,
                          NetworkId, CombatStats>();
    for (auto e : view) {
        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        if (dead) continue;

        auto &ai = view.get<BotAIState>(e);
        if (ai.TargetEntity == entt::null) continue;
        if (!reg.valid(ai.TargetEntity)) continue;

        auto &stats = view.get<CombatStats>(e);
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &net = view.get<NetworkId>(e);

        ArrowSpawnContext ctx{
            cb, id_state, now,
            pos.Value, angle.Radians,
            net.Value, e,
            stats.Atk
        };
        try_fire(stats, ctx);
    }
}

} // namespace sim
