#pragma once

#include "../systems/timed_modifiers.h"
#include "../game_config.h"
#include "skill_interface.h"
#include <algorithm>
#include <entt/entt.hpp>

namespace sim {

class TerrainRushSkill : public ISkill {
  public:
    explicit TerrainRushSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return _tuning.Id; }
    SkillKind kind() const override { return SkillKind::TerrainRush; }
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
        CommandBuffer &,
        IdState &,
        int
    ) override {
        apply_timed_modifier(
            reg,
            caster,
            TimedModifierType::MoveSpeedMultiplier,
            id(),
            _tuning.ModifierMagnitude,
            _tuning.ModifierDuration
        );
        apply_timed_modifier(
            reg,
            caster,
            TimedModifierType::IgnoreTerrain,
            id(),
            1.0f,
            _tuning.ModifierDuration
        );
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
