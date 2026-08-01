#pragma once

#include "../arrow_spawner.h"
#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <cmath>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class ChannelBurstSkill : public ISkill {
  public:
    explicit ChannelBurstSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return 4; }
    SkillKind kind() const override { return SkillKind::ChannelBurst; }

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
    float effect_value(int level) const override {
        return _tuning.EffectBase + (level - 1) * _tuning.EffectPerLevel;
    }
    float damage(int level, float atk) const override {
        return _tuning.DamageBase + (level - 1) * _tuning.DamagePerLevel +
               atk * _tuning.DamageAtkRatio;
    }

    int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) override {
        return 0;
    }

    bool can_enter_casting(
        entt::registry &reg, entt::entity caster, const CastState &cs, int level
    ) override {
        return true;
    }

    void on_cast_complete(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        int level
    ) override {
        cs.Timer = effect_value(level);
        cs.SubTimer = 0.0f;
    }

    void on_channel_tick(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        int level,
        float dt
    ) override {
        cs.SubTimer -= dt;
        if (cs.SubTimer > 0.0f)
            return;

        float interval = _tuning.ChannelInterval;
        cs.SubTimer = interval;

        bool is_bot = reg.all_of<BotTag>(caster);
        float dmg = damage(level, reg.get<CombatStats>(caster).Atk);
        if (is_bot)
            dmg *= stats(reg).BotSkillDmgMul;

        int count = _tuning.ChannelProjectileCount;
        float spawn_radius = _tuning.ChannelProjectileSpawnRadius;
        float arrow_speed = stats(reg).ArrowSpeed;
        float arrow_lifetime = stats(reg).ArrowLifetime;
        auto &pos = reg.get<Position2D>(caster);
        int net_id = reg.get<NetworkId>(caster).Value;
        entt::entity owner = caster;

        for (int i = 0; i < count; ++i) {
            float angle = (float)i * (2.0f * 3.14159265f) / (float)count;
            Vec2 dir{std::cos(angle), std::sin(angle)};
            Vec2 spawn_pos = pos.Value + dir * spawn_radius;
            int arrow_id = ids.NextArrowId++;
            cb.push([arrow_id,
                     spawn_pos,
                     angle,
                     net_id,
                     owner,
                     dmg,
                     arrow_speed,
                     arrow_lifetime](entt::registry &r) {
                auto a = r.create();
                Vec2 vel{std::cos(angle), std::sin(angle)};
                vel *= arrow_speed;
                r.emplace<Position2D>(a, spawn_pos);
                r.emplace<Velocity2D>(a, vel);
                r.emplace<FacingAngle>(a, angle);
                r.emplace<ArrowTag>(a, net_id, owner, dmg, 1.0f);
                r.emplace<Lifetime>(a, arrow_lifetime);
                r.emplace<NetworkId>(a, arrow_id);
            });
        }
    }

    bool can_interrupt(CastState::Phase phase) const override {
        return phase == CastState::Phase::Chasing;
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
