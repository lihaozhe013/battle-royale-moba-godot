#pragma once

#include "../components.h"
#include "../game_config.h"
#include <algorithm>
#include <entt/entt.hpp>

namespace sim {

inline void apply_timed_modifier(
    entt::registry &reg,
    entt::entity entity,
    TimedModifierType type,
    int source_id,
    float magnitude,
    float duration
) {
    if (!reg.valid(entity) || duration <= 0.0f)
        return;

    auto &modifiers = reg.get_or_emplace<TimedModifiers>(entity);
    for (auto &modifier : modifiers.Values) {
        if (modifier.Type == type && modifier.SourceId == source_id) {
            modifier.Magnitude = magnitude;
            modifier.Remaining = duration;
            return;
        }
    }
    modifiers.Values.push_back({type, source_id, magnitude, duration});
}

inline bool has_timed_modifier(
    const entt::registry &reg, entt::entity entity, TimedModifierType type
) {
    if (!reg.valid(entity) || !reg.all_of<TimedModifiers>(entity))
        return false;
    for (const auto &modifier : reg.get<TimedModifiers>(entity).Values) {
        if (modifier.Type == type && modifier.Remaining > 0.0f)
            return true;
    }
    return false;
}

inline float timed_modifier_product(
    const entt::registry &reg, entt::entity entity, TimedModifierType type
) {
    if (!reg.valid(entity) || !reg.all_of<TimedModifiers>(entity))
        return 1.0f;
    float product = 1.0f;
    for (const auto &modifier : reg.get<TimedModifiers>(entity).Values) {
        if (modifier.Type == type && modifier.Remaining > 0.0f)
            product *= modifier.Magnitude;
    }
    return product;
}

inline float effective_move_speed(
    const entt::registry &reg, entt::entity entity, float base_speed
) {
    return std::max(
        0.0f,
        base_speed *
            timed_modifier_product(
                reg, entity, TimedModifierType::MoveSpeedMultiplier
            )
    );
}

inline float effective_attack_speed(
    const entt::registry &reg, entt::entity entity, float base_speed
) {
    return std::clamp(
        base_speed *
            timed_modifier_product(
                reg, entity, TimedModifierType::AttackSpeedMultiplier
            ),
        0.01f,
        stats(reg).AspMax
    );
}

inline void timed_modifier_system(entt::registry &reg, float dt) {
    auto view = reg.view<TimedModifiers>();
    for (auto entity : view) {
        auto &modifiers = view.get<TimedModifiers>(entity);
        bool dead = reg.all_of<Dead>(entity) && reg.get<Dead>(entity).enabled;
        if (dead) {
            modifiers.Values.clear();
            continue;
        }
        for (auto &modifier : modifiers.Values)
            modifier.Remaining -= dt;
        modifiers.Values.erase(
            std::remove_if(
                modifiers.Values.begin(),
                modifiers.Values.end(),
                [](const TimedModifier &modifier) {
                    return modifier.Remaining <= 0.0f;
                }
            ),
            modifiers.Values.end()
        );
    }
}

} // namespace sim
