#pragma once

#include "../command_buffer.h"
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "match_stats.h"
#include <entt/entt.hpp>

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
            if (target == arrow_tag.OwnerEntity)
                continue;
            if (reg.all_of<Dead>(target) && reg.get<Dead>(target).enabled)
                continue;

            // Homing 箭矢只检测锁定目标
            if (reg.all_of<Homing>(arrow)) {
                auto &hom = reg.get<Homing>(arrow);
                if (target != hom.Target)
                    continue;
            }

            float target_radius = reg.all_of<BotTag>(target)
                                      ? stats(reg).BotRadius
                                      : stats(reg).PlayerRadius;

            if (!circles_overlap(
                    arrow_pos.Value,
                    stats(reg).ArrowRadius,
                    target_view.get<Position2D>(target).Value,
                    target_radius
                )) {
                continue;
            }

            int victim_id = reg.all_of<NetworkId>(target)
                                ? reg.get<NetworkId>(target).Value
                                : 0;

            // Hit!
            int actual_damage = apply_damage(
                reg,
                arrow_tag.OwnerEntity,
                target,
                static_cast<int>(arrow_tag.Dmg)
            );

            auto impact_view = reg.view<ImpactEventBuffer>();
            for (auto impact_entity : impact_view) {
                impact_view.get<ImpactEventBuffer>(impact_entity)
                    .events.push_back({
                        arrow_tag.OwnerId,
                        victim_id,
                        arrow_tag.SourceSkillId,
                    });
            }

            // Lifesteal
            if (arrow_tag.LifestealRatio > 0.0f &&
                arrow_tag.OwnerEntity != entt::null &&
                reg.valid(arrow_tag.OwnerEntity) &&
                reg.all_of<Health>(arrow_tag.OwnerEntity)) {
                int heal =
                    static_cast<int>(actual_damage * arrow_tag.LifestealRatio);
                apply_healing(reg, arrow_tag.OwnerEntity, heal);
            }

            // Destroy arrow
            cb.push([arrow](entt::registry &r) { r.destroy(arrow); });
            break;
        }
    }
}

} // namespace sim
