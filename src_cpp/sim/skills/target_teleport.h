#pragma once

#include "../systems/timed_modifiers.h"
#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>

namespace sim {

class TargetTeleportSkill : public ISkill {
  public:
    explicit TargetTeleportSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return _tuning.Id; }
    SkillKind kind() const override { return SkillKind::TargetTeleport; }
    SkillTargetMode target_mode() const override {
        return SkillTargetMode::Unit;
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

    int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) override {
        if (ctx.target_entity == entt::null || ctx.target_entity == caster ||
            !reg.valid(ctx.target_entity) ||
            !reg.all_of<Damageable>(ctx.target_entity) ||
            !reg.all_of<Position2D>(ctx.target_entity))
            return 4;
        if (reg.all_of<Dead>(ctx.target_entity) &&
            reg.get<Dead>(ctx.target_entity).enabled)
            return 5;
        return 0;
    }

    bool can_enter_casting(
        entt::registry &reg, entt::entity caster, const CastState &cs, int level
    ) override {
        if (cs.TargetEntity == entt::null || cs.TargetEntity == caster ||
            !reg.valid(cs.TargetEntity) ||
            !reg.all_of<Damageable>(cs.TargetEntity) ||
            !reg.all_of<Position2D>(cs.TargetEntity))
            return false;
        if (reg.all_of<Dead>(cs.TargetEntity) &&
            reg.get<Dead>(cs.TargetEntity).enabled)
            return false;
        Vec2 delta = reg.get<Position2D>(cs.TargetEntity).Value -
                     reg.get<Position2D>(caster).Value;
        return vec2_length_sq(delta) <= range(level) * range(level);
    }

    float range(int level) const override {
        return base_range(level) + (level - 1) * _tuning.RangePerLevel;
    }

    void on_cast_complete(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &,
        IdState &,
        int level
    ) override {
        if (cs.TargetEntity == entt::null || cs.TargetEntity == caster ||
            !reg.valid(cs.TargetEntity) ||
            !reg.all_of<Damageable, Position2D>(cs.TargetEntity) ||
            (reg.all_of<Dead>(cs.TargetEntity) &&
             reg.get<Dead>(cs.TargetEntity).enabled))
            return;
        auto target = cs.TargetEntity;
        Vec2 target_pos = reg.get<Position2D>(target).Value;
        Vec2 delta = target_pos - reg.get<Position2D>(caster).Value;
        if (vec2_length_sq(delta) > range(level) * range(level))
            return;
        reg.get<Position2D>(caster).Value = target_pos;
        if (reg.all_of<FacingAngle>(caster) &&
            reg.all_of<NetworkId>(target)) {
            if (vec2_length_sq(delta) > 0.001f)
                reg.get<FacingAngle>(caster).Radians =
                    std::atan2(delta.y, delta.x);
            cs.HitTargetId = reg.get<NetworkId>(target).Value;
        }
        apply_timed_modifier(
            reg,
            caster,
            TimedModifierType::AttackSpeedMultiplier,
            id(),
            _tuning.ModifierMagnitude,
            _tuning.ModifierDuration
        );
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
