#pragma once

#include "../arrow_spawner.h"
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "match_stats.h"
#include "timed_modifiers.h"
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>
#include <random>

namespace sim {

inline void emit_attack_started(
    entt::registry &reg, int attacker_id, int target_id = -1
) {
    auto view = reg.view<AttackStartedEventBuffer>();
    for (auto entity : view)
        view.get<AttackStartedEventBuffer>(entity)
            .events.push_back({attacker_id, target_id});
}

inline void emit_impact(
    entt::registry &reg,
    int attacker_id,
    int victim_id,
    int source_skill_id,
    int damage,
    int healing,
    bool critical
) {
    auto view = reg.view<ImpactEventBuffer>();
    for (auto entity : view)
        view.get<ImpactEventBuffer>(entity).events.push_back(
            {attacker_id,
             victim_id,
             source_skill_id,
             damage,
             healing,
             critical}
        );
}

inline float health_missing_ratio(
    entt::registry &reg, entt::entity entity
) {
    if (!reg.valid(entity) || !reg.all_of<Health>(entity))
        return 0.0f;
    const auto &health = reg.get<Health>(entity);
    if (health.Max <= 0)
        return 0.0f;
    return std::clamp(
        1.0f - static_cast<float>(health.Cur) / health.Max, 0.0f, 1.0f
    );
}

inline bool roll_critical(
    entt::registry &reg, entt::entity attacker, std::mt19937 &rng
) {
    if (!reg.all_of<BasicAttackPassive>(attacker))
        return false;
    const auto &passive = reg.get<BasicAttackPassive>(attacker);
    float chance = passive.CritChanceMin +
                   (passive.CritChanceMax - passive.CritChanceMin) *
                       health_missing_ratio(reg, attacker);
    return std::bernoulli_distribution(std::clamp(chance, 0.0f, 1.0f))(rng);
}

inline float current_lifesteal(
    entt::registry &reg, entt::entity attacker
) {
    if (!reg.all_of<BasicAttackPassive>(attacker))
        return 0.0f;
    const auto &passive = reg.get<BasicAttackPassive>(attacker);
    return passive.LifestealMin +
           (passive.LifestealMax - passive.LifestealMin) *
               health_missing_ratio(reg, attacker);
}

inline float crit_multiplier(entt::registry &reg, entt::entity attacker) {
    return reg.all_of<BasicAttackPassive>(attacker)
               ? reg.get<BasicAttackPassive>(attacker).CritMultiplier
               : 1.0f;
}

inline bool try_basic_attack(
    entt::registry &reg,
    entt::entity attacker,
    entt::entity target,
    double now,
    std::mt19937 &rng
) {
    auto &combat = reg.get<CombatStats>(attacker);
    float attack_speed = effective_attack_speed(reg, attacker, combat.Asp);
    double interval = 1.0 / std::max(0.01f, attack_speed);
    if (now - combat.LastFireTime < interval)
        return false;
    combat.LastFireTime = now;

    int attacker_id = reg.get<NetworkId>(attacker).Value;
    int victim_id = reg.all_of<NetworkId>(target)
                        ? reg.get<NetworkId>(target).Value
                        : 0;
    emit_attack_started(reg, attacker_id, victim_id);

    bool critical = roll_critical(reg, attacker, rng);
    float damage_value = combat.Atk;
    if (critical)
        damage_value *= crit_multiplier(reg, attacker);
    int requested_damage = std::max(1, static_cast<int>(damage_value));
    int actual_damage = apply_damage(reg, attacker, target, requested_damage);
    int healing = 0;
    float lifesteal = current_lifesteal(reg, attacker);
    if (actual_damage > 0 && lifesteal > 0.0f)
        healing = apply_healing(
            reg,
            attacker,
            static_cast<int>(actual_damage * lifesteal)
        );
    emit_impact(
        reg,
        attacker_id,
        victim_id,
        0,
        actual_damage,
        healing,
        critical
    );
    return true;
}

inline void attack_fire_system(
    entt::registry &reg,
    double now,
    CommandBuffer &cb,
    IdState &ids,
    std::mt19937 &rng
) {
    auto view =
        reg.view<PlayerTag, Position2D, CombatStats, NetworkId, AttackTarget>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal && !reg.all_of<BotTag>(e))
            continue;

        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
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
        if (at.Target == entt::null || !reg.valid(at.Target))
            continue;
        bool target_dead = reg.all_of<Dead>(at.Target) &&
                           reg.get<Dead>(at.Target).enabled;
        if (target_dead || !reg.all_of<Position2D>(at.Target))
            continue;

        auto &pos = view.get<Position2D>(e);
        auto &target_pos = reg.get<Position2D>(at.Target).Value;
        Vec2 delta = target_pos - pos.Value;
        float dist = glm::length(delta);
        float attack_range = reg.all_of<AttackProfile>(e)
                                 ? reg.get<AttackProfile>(e).Range
                                 : 0.0f;
        if (dist > attack_range)
            continue;

        float aim_angle = std::atan2(delta.y, delta.x);
        auto &combat = view.get<CombatStats>(e);
        float attack_speed = effective_attack_speed(reg, e, combat.Asp);
        auto &net = view.get<NetworkId>(e);
        AttackDelivery delivery = reg.all_of<AttackProfile>(e)
                                      ? reg.get<AttackProfile>(e).Delivery
                                      : AttackDelivery::Projectile;

        if (delivery == AttackDelivery::Melee) {
            try_basic_attack(reg, e, at.Target, now, rng);
        } else {
            bool critical = roll_critical(reg, e, rng);
            float damage = combat.Atk;
            if (critical)
                damage *= crit_multiplier(reg, e);
            ArrowSpawnContext ctx{
                cb,
                ids,
                now,
                pos.Value,
                aim_angle,
                net.Value,
                e,
                damage,
                current_lifesteal(reg, e),
                0,
                at.Target,
                at.TargetNetworkId,
                attack_speed,
            };
            ctx.critical = critical;
            if (try_fire(combat, ctx))
                emit_attack_started(reg, net.Value, at.TargetNetworkId);
        }

        if (reg.all_of<FacingAngle>(e))
            reg.get<FacingAngle>(e).Radians = aim_angle;
    }
}

// Compatibility overload for callers that used the pre-crit API. The world
// owns and passes its seeded RNG; legacy standalone callers get an equivalent
// deterministic stream without having to change their call sites.
inline void attack_fire_system(
    entt::registry &reg, double now, CommandBuffer &cb, IdState &ids
) {
    static thread_local std::mt19937 legacy_rng{42};
    attack_fire_system(reg, now, cb, ids, legacy_rng);
}

} // namespace sim
