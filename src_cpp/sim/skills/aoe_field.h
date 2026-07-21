#pragma once

#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class AoEFieldSkill : public ISkill {
  public:
    int id() const override { return 2; }
    SkillKind kind() const override { return SkillKind::AoEField; }

    float base_cooldown() const override { return 20.0f; }
    float base_mana_cost() const override { return 100.0f; }
    float base_cast_time() const override { return 0.5f; }
    float base_range(int) const override { return 4.0f; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * 1.5f;
    }
    float mana_cost(int level) const override {
        return base_mana_cost() * std::max(0.3f, 1.0f - (level - 1) * 0.10f);
    }
    float damage(int level, float atk) const override {
        return 35.0f + (level - 1) * 10.0f + atk * 0.9f;
    }
    float effect_value(int level) const override {
        return 2.0f + (level - 1) * 0.25f;
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
            skill_dmg *= GameConfig::BotSkillDmgMul;

        auto target_view = reg.view<Damageable, Position2D, Health>();
        for (auto t : target_view) {
            if (t == caster)
                continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(t).Value - cs.AimPos;
            if (vec2_length_sq(delta) > range(level) * range(level))
                continue;

            auto &hp = target_view.get<Health>(t);
            hp.Cur -= static_cast<int>(skill_dmg);
            if (hp.Cur <= 0) {
                hp.Cur = 0;
                if (reg.all_of<Dead>(t))
                    reg.get<Dead>(t).enabled = true;
                if (reg.all_of<BotAIState>(t))
                    reg.get<BotAIState>(t).RespawnTimer =
                        GameConfig::BotRespawnTime;
                int victim_id =
                    reg.all_of<NetworkId>(t) ? reg.get<NetworkId>(t).Value : 0;
                auto kill_view = reg.view<KillEventBuffer>();
                for (auto k : kill_view)
                    kill_view.get<KillEventBuffer>(k).events.push_back(
                        {reg.get<NetworkId>(caster).Value, victim_id}
                    );
                if (reg.all_of<Kills>(caster))
                    reg.get<Kills>(caster).Value += 1;
            } else {
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
};

} // namespace sim
