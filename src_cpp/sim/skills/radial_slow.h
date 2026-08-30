#pragma once

#include "../components.h"
#include "../systems/timed_modifiers.h"
#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <algorithm>
#include <entt/entt.hpp>

namespace sim {

class RadialSlowSkill : public ISkill {
  public:
    explicit RadialSlowSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return _tuning.Id; }
    SkillKind kind() const override { return SkillKind::RadialSlow; }
    SkillTargetMode target_mode() const override {
        return SkillTargetMode::Self;
    }
    int max_level() const override { return _tuning.MaxLevel; }

    float base_cooldown() const override { return _tuning.BaseCooldown; }
    float base_mana_cost() const override { return _tuning.BaseManaCost; }
    float base_cast_time() const override { return _tuning.BaseCastTime; }
    float base_range(int) const override { return _tuning.BaseRange; }

    float cooldown(int level) const override {
        return std::max(
            0.0f,
            base_cooldown() - (level - 1) * _tuning.CooldownPerLevel
        );
    }
    float mana_cost(int) const override { return base_mana_cost(); }
    float effect_value(int) const override { return _tuning.ModifierDuration; }

    int validate_cast(
        entt::registry &, entt::entity, const CastContext &
    ) override {
        return 0;
    }

    bool can_enter_casting(
        entt::registry &, entt::entity, const CastState &, int
    ) override {
        return true;
    }

    void on_cast_complete(
        entt::registry &reg,
        entt::entity caster,
        CastState &,
        CommandBuffer &cb,
        IdState &ids,
        int level
    ) override {
        Vec2 center = reg.get<Position2D>(caster).Value;
        float radius = range(level);
        float radius_sq = radius * radius;
        int source_id = reg.all_of<NetworkId>(caster)
                            ? reg.get<NetworkId>(caster).Value
                            : id();
        auto target_view = reg.view<Damageable, Position2D>();
        for (auto target : target_view) {
            if (target == caster)
                continue;
            if (reg.all_of<Dead>(target) && reg.get<Dead>(target).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(target).Value - center;
            if (vec2_length_sq(delta) > radius_sq)
                continue;
            apply_timed_modifier(
                reg,
                target,
                TimedModifierType::MoveSpeedMultiplier,
                source_id,
                _tuning.ModifierMagnitude,
                _tuning.ModifierDuration
            );
        }

        int owner_id = reg.all_of<NetworkId>(caster)
                           ? reg.get<NetworkId>(caster).Value
                           : 0;
        int aoe_id = ids.NextAoEId++;
        int skill_id = id();
        cb.push([center, radius, owner_id, aoe_id, skill_id](entt::registry &r) {
            auto area = r.create();
            r.emplace<Position2D>(area, center);
            r.emplace<AoETag>(area, owner_id, skill_id, radius, 0.5f, 0.5f);
            r.emplace<NetworkId>(area, aoe_id);
        });
    }

    float range(int level) const override {
        return base_range(level) + (level - 1) * _tuning.RangePerLevel;
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
