#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../arrow_spawner.h"

namespace sim {

inline void player_fire_system(entt::registry &reg, double now,
                               CommandBuffer &cb, IdState &id_state) {
    auto view = reg.view<PlayerTag, Position2D, FacingAngle, PlayerInputState,
                          CombatStats, NetworkId>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        if (!view.get<PlayerInputState>(e).Fire) continue;

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
