#pragma once

#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include "../systems/match_stats.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class MeleeStrikeSkill : public ISkill {
  public:
    explicit MeleeStrikeSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return _tuning.Id; }
    SkillKind kind() const override { return SkillKind::MeleeSingle; }
    SkillTargetMode target_mode() const override { return SkillTargetMode::Unit; }
    int max_level() const override { return _tuning.MaxLevel; }

    float base_cooldown() const override { return _tuning.BaseCooldown; }
    float base_mana_cost() const override { return _tuning.BaseManaCost; }
    float base_cast_time() const override { return _tuning.BaseCastTime; }
    float base_range(int) const override { return _tuning.BaseRange; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * _tuning.CooldownPerLevel;
    }
    float mana_cost(int level) const override {
        return base_mana_cost() *
               std::max(
                   _tuning.ManaReductionMin,
                   1.0f - (level - 1) * _tuning.ManaReductionPerLevel
               );
    }
    float damage(int level, float atk) const override {
        return _tuning.DamageBase + (level - 1) * _tuning.DamagePerLevel +
               atk * _tuning.DamageAtkRatio;
    }

    int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) override {
        if (ctx.target_entity == entt::null || ctx.target_entity == caster ||
            !reg.valid(ctx.target_entity) ||
            !reg.all_of<Damageable>(ctx.target_entity))
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
            !reg.all_of<Damageable>(cs.TargetEntity))
            return false;
        bool dead = reg.all_of<Dead>(cs.TargetEntity) &&
                    reg.get<Dead>(cs.TargetEntity).enabled;
        if (dead)
            return false;
        Vec2 delta = reg.get<Position2D>(cs.TargetEntity).Value -
                     reg.get<Position2D>(caster).Value;
        return vec2_length_sq(delta) <= range(level) * range(level);
    }

    void on_cast_complete(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        int level
    ) override {
        entt::entity tgt = cs.TargetEntity;
        if (!reg.valid(tgt))
            return;
        float dmg = damage(level, reg.get<CombatStats>(caster).Atk);
        bool is_bot = reg.all_of<BotTag>(caster);
        if (is_bot)
            dmg *= stats(reg).BotSkillDmgMul;

        apply_damage(reg, caster, tgt, static_cast<int>(dmg));

        if (reg.all_of<NetworkId>(tgt))
            cs.HitTargetId = reg.get<NetworkId>(tgt).Value;
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
