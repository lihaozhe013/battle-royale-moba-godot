#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "../command_buffer.h"

namespace sim {

inline void combat_system(entt::registry &reg, CommandBuffer &cb) {
    auto arrow_view = reg.view<ArrowTag, NetworkId, Position2D, Lifetime>();
    auto target_view = reg.view<Damageable, Position2D, Health>();

    for (auto arrow : arrow_view) {
        auto &arrow_tag = arrow_view.get<ArrowTag>(arrow);
        auto &arrow_pos = arrow_view.get<Position2D>(arrow);
        auto &arrow_life = arrow_view.get<Lifetime>(arrow);

        // Expired arrow
        if (arrow_life.Remaining <= 0.0f) {
            cb.push([arrow](entt::registry &r) { r.destroy(arrow); });
            continue;
        }

        for (auto target : target_view) {
            if (target == arrow_tag.OwnerEntity) continue;
            if (reg.all_of<Dead>(target) && reg.get<Dead>(target).enabled) continue;

            float target_radius = reg.all_of<BotTag>(target)
                ? GameConfig::BotRadius : GameConfig::PlayerRadius;

            if (!circles_overlap(arrow_pos.Value, GameConfig::ArrowRadius,
                                 target_view.get<Position2D>(target).Value, target_radius)) {
                continue;
            }

            // Hit!
            auto &hp = target_view.get<Health>(target);
            hp.Cur -= static_cast<int>(arrow_tag.Dmg);

            if (hp.Cur <= 0) {
                hp.Cur = 0;
                if (reg.all_of<Dead>(target)) {
                    reg.get<Dead>(target).enabled = true;
                }
                if (reg.all_of<BotAIState>(target)) {
                    reg.get<BotAIState>(target).RespawnTimer = GameConfig::BotRespawnTime;
                }
            }

            // Kill event
            if (hp.Cur <= 0) {
                int victim_id = reg.all_of<NetworkId>(target)
                    ? reg.get<NetworkId>(target).Value : 0;
                auto kill_view = reg.view<KillEventBuffer>();
                for (auto k : kill_view) {
                    kill_view.get<KillEventBuffer>(k).events.push_back({
                        arrow_tag.OwnerId,
                        victim_id
                    });
                }

                // Increment killer kills
                if (reg.valid(arrow_tag.OwnerEntity) && reg.all_of<Kills>(arrow_tag.OwnerEntity)) {
                    reg.get<Kills>(arrow_tag.OwnerEntity).Value += 1;
                }
            }

            // Destroy arrow
            cb.push([arrow](entt::registry &r) { r.destroy(arrow); });
            break;
        }
    }
}

} // namespace sim
