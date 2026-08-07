#pragma once

#include "command_buffer.h"
#include "components.h"
#include "game_config.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

struct ArrowSpawnContext {
    CommandBuffer &cb;
    IdState &id_state;
    double now;
    Vec2 spawn_pos;
    float angle;
    int owner_id;
    entt::entity owner_entity;
    float dmg;
    float lifesteal_ratio = 0.0f;
    int source_skill_id = 0;
    entt::entity homing_target = entt::null;
    int homing_target_net_id = -1;
};

inline bool try_fire(CombatStats &stats, const ArrowSpawnContext &ctx) {
    double interval = 1.0 / stats.Asp;
    if (ctx.now - stats.LastFireTime < interval) {
        return false;
    }
    stats.LastFireTime = ctx.now;

    int arrow_id = ctx.id_state.NextArrowId++;

    ctx.cb.push([arrow_id, ctx](entt::registry &reg) {
        auto e = reg.create();
        Vec2 vel{
            std::cos(ctx.angle) * sim::stats(reg).ArrowSpeed,
            std::sin(ctx.angle) * sim::stats(reg).ArrowSpeed
        };
        reg.emplace<Position2D>(e, ctx.spawn_pos);
        reg.emplace<Velocity2D>(e, vel);
        reg.emplace<FacingAngle>(e, ctx.angle);
        reg.emplace<ArrowTag>(
            e,
            ctx.owner_id,
            ctx.owner_entity,
            ctx.dmg,
            ctx.lifesteal_ratio,
            ctx.source_skill_id
        );
        reg.emplace<Lifetime>(e, sim::stats(reg).ArrowLifetime);
        reg.emplace<NetworkId>(e, arrow_id);
        if (ctx.homing_target != entt::null) {
            reg.emplace<Homing>(e, ctx.homing_target, ctx.homing_target_net_id);
        }
    });

    return true;
}

} // namespace sim
