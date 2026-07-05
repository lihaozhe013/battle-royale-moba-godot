#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "components.h"
#include "game_config.h"
#include "command_buffer.h"

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
            std::cos(ctx.angle) * GameConfig::ArrowSpeed,
            std::sin(ctx.angle) * GameConfig::ArrowSpeed
        };
        reg.emplace<Position2D>(e, ctx.spawn_pos);
        reg.emplace<Velocity2D>(e, vel);
        reg.emplace<FacingAngle>(e, ctx.angle);
        reg.emplace<ArrowTag>(e, ctx.owner_id, ctx.owner_entity, ctx.dmg);
        reg.emplace<Lifetime>(e, GameConfig::ArrowLifetime);
        reg.emplace<NetworkId>(e, arrow_id);
    });

    return true;
}

} // namespace sim
