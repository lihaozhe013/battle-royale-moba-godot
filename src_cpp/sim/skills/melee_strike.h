#pragma once

#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class MeleeStrikeSkill : public ISkill {
  public:
    int id() const override { return 1; }
    SkillKind kind() const override { return SkillKind::MeleeSingle; }

    float base_cooldown() const override { return 5.0f; }
    float base_mana_cost() const override { return 20.0f; }
    float base_cast_time() const override { return 0.2f; }
    float base_range(int) const override { return 8.0f; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * 0.5f;
    }
    float mana_cost(int level) const override {
        return base_mana_cost() * std::max(0.3f, 1.0f - (level - 1) * 0.05f);
    }
    float damage(int level, float atk) const override {
        return 40.0f + (level - 1) * 15.0f + atk * 0.9f;
    }

    int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) override {
        if (ctx.target_entity == entt::null || !reg.valid(ctx.target_entity))
            return 4;
        if (reg.all_of<Dead>(ctx.target_entity) &&
            reg.get<Dead>(ctx.target_entity).enabled)
            return 5;
        return 0;
    }

    bool can_enter_casting(
        entt::registry &reg, entt::entity caster, const CastState &cs, int level
    ) override {
        if (cs.TargetEntity == entt::null || !reg.valid(cs.TargetEntity))
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
            dmg *= GameConfig::BotSkillDmgMul;

        auto &hp = reg.get<Health>(tgt);
        hp.Cur -= static_cast<int>(dmg);

        if (reg.all_of<NetworkId>(tgt))
            cs.HitTargetId = reg.get<NetworkId>(tgt).Value;

        if (hp.Cur <= 0) {
            hp.Cur = 0;
            if (reg.all_of<Dead>(tgt))
                reg.get<Dead>(tgt).enabled = true;
            if (reg.all_of<BotAIState>(tgt))
                reg.get<BotAIState>(tgt).RespawnTimer =
                    GameConfig::BotRespawnTime;
            int victim_id =
                reg.all_of<NetworkId>(tgt) ? reg.get<NetworkId>(tgt).Value : 0;
            auto kill_view = reg.view<KillEventBuffer>();
            for (auto k : kill_view)
                kill_view.get<KillEventBuffer>(k).events.push_back(
                    {reg.get<NetworkId>(caster).Value, victim_id}
                );
            if (reg.all_of<Kills>(caster))
                reg.get<Kills>(caster).Value += 1;
        }
    }
};

} // namespace sim
