#pragma once

#include "../command_buffer.h"
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "xp_helper.h"
#include <cmath>
#include <entt/entt.hpp>

namespace sim {

inline void pickup_system(
    entt::registry &reg, float dt, CommandBuffer &cb, IdState &id_state
) {
    // Phase 1: Spawner tick
    auto spawner_view = reg.view<PickupSpawner>();
    for (auto s : spawner_view) {
        auto &sp = spawner_view.get<PickupSpawner>(s);
        if (sp.Active)
            continue;
        sp.CurrentTimer -= dt;
        if (sp.CurrentTimer > 0.0f)
            continue;

        int id = id_state.NextPickupId++;
        sp.Active = true;
        sp.CurrentEntityId = id;

        cb.push([sp, id](entt::registry &r) {
            auto e = r.create();
            r.emplace<NetworkId>(e, id);
            r.emplace<Position2D>(e, sp.Position);
            r.emplace<PickupTag>(e, sp.Type, sp.Value);
        });
    }

    // Phase 2: Mover overlap
    auto pickup_view = reg.view<PickupTag, Position2D, NetworkId>();
    auto mover_view = reg.view<Damageable, Position2D>();

    for (auto pickup : pickup_view) {
        auto &p_tag = pickup_view.get<PickupTag>(pickup);
        auto &p_pos = pickup_view.get<Position2D>(pickup);
        int p_id = pickup_view.get<NetworkId>(pickup).Value;

        for (auto mover : mover_view) {
            if (mover == pickup)
                continue;
            bool dead = reg.all_of<Dead>(mover) && reg.get<Dead>(mover).enabled;
            if (dead)
                continue;

            float mover_radius = reg.all_of<BotTag>(mover)
                                     ? GameConfig::BotRadius
                                     : GameConfig::PlayerRadius;

            if (!circles_overlap(
                    p_pos.Value,
                    GameConfig::PickupRadius,
                    mover_view.get<Position2D>(mover).Value,
                    mover_radius
                )) {
                continue;
            }

            // Apply pickup effect
            if (p_tag.Type == PickupType::Xp) {
                apply_xp(reg, mover, p_tag.Value);
            } else if (
                (p_tag.Type == PickupType::Heal ||
                 p_tag.Type == PickupType::SmallHeal) &&
                reg.all_of<Health>(mover)
            ) {
                auto &hp = reg.get<Health>(mover);
                float fraction =
                    (p_tag.Type == PickupType::Heal)
                        ? GameConfig::HealFraction
                        : (GameConfig::SmallHealPickupValue / 100.0f);
                int heal = static_cast<int>(std::ceil(hp.Max * fraction));
                hp.Cur = std::min(hp.Cur + heal, hp.Max);
            }

            // Destroy pickup
            cb.push([pickup](entt::registry &r) { r.destroy(pickup); });

            // Deactivate spawner
            for (auto s : spawner_view) {
                auto &sp = spawner_view.get<PickupSpawner>(s);
                if (sp.Active && sp.CurrentEntityId == p_id) {
                    sp.Active = false;
                    sp.CurrentTimer = sp.RespawnTime;
                    sp.CurrentEntityId = 0;
                    break;
                }
            }
            break;
        }
    }
}

} // namespace sim
