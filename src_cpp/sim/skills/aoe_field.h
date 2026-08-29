#pragma once

#include "../game_config.h"
#include "../systems/match_stats.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class AoEFieldSkill : public ISkill {
  public:
    explicit AoEFieldSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return 2; }
    SkillKind kind() const override { return SkillKind::AoEField; }

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
    float effect_value(int level) const override {
        return _tuning.EffectBase + (level - 1) * _tuning.EffectPerLevel;
    }

    int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) override {
        if (glm::length(ctx.aim_pos) < 0.001f)
            return 4;
        return 0;
    }

    bool can_enter_casting(
        entt::registry &reg, entt::entity caster, const CastState &cs, int level
    ) override {
        Vec2 delta = cs.AimPos - reg.get<Position2D>(caster).Value;
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
        bool is_bot = reg.all_of<BotTag>(caster);
        float skill_dmg = damage(level, reg.get<CombatStats>(caster).Atk);
        if (is_bot)
            skill_dmg *= stats(reg).BotSkillDmgMul;

        auto target_view = reg.view<Damageable, Position2D, Health>();
        for (auto t : target_view) {
            if (t == caster)
                continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(t).Value - cs.AimPos;
            if (vec2_length_sq(delta) > range(level) * range(level))
                continue;

            int actual_damage =
                apply_damage(reg, caster, t, static_cast<int>(skill_dmg));
            if (actual_damage > 0 && reg.get<Health>(t).Cur > 0) {
                auto &st = reg.get_or_emplace<StatusEffect>(t);
                st.Type = StatusType::Stun;
                st.Timer = effect_value(level);
            }
        }

        auto aoe = reg.create();
        reg.emplace<Position2D>(aoe, cs.AimPos);
        reg.emplace<AoETag>(
            aoe,
            reg.get<NetworkId>(caster).Value,
            cs.SkillId,
            range(level),
            effect_value(level),
            effect_value(level)
        );
        reg.emplace<NetworkId>(aoe, ids.NextAoEId++);
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
