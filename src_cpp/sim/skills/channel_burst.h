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
    int id() const override { return 4; }
    SkillKind kind() const override { return SkillKind::ChannelBurst; }

    float base_cooldown() const override { return 60.0f; }
    float base_mana_cost() const override { return 230.0f; }
    float base_cast_time() const override { return 1.0f; }
    float base_range(int) const override { return 0.0f; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * 5.0f;
    }
    float mana_cost(int level) const override {
        return base_mana_cost() * std::max(0.3f, 1.0f - (level - 1) * 0.03f);
    }
    float effect_value(int level) const override {
        return 5.0f + (level - 1) * 0.5f;
    }
    float damage(int level, float atk) const override {
        return 0.0f + atk * 0.9f;
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

        float interval = 0.5f;
        cs.SubTimer = interval;

        bool is_bot = reg.all_of<BotTag>(caster);
        float dmg = damage(level, reg.get<CombatStats>(caster).Atk);
        if (is_bot)
            dmg *= GameConfig::BotSkillDmgMul;

        int count = 16;
        auto &pos = reg.get<Position2D>(caster);
        int net_id = reg.get<NetworkId>(caster).Value;
        entt::entity owner = caster;

        for (int i = 0; i < count; ++i) {
            float angle = (float)i * (2.0f * 3.14159265f) / (float)count;
            Vec2 dir{std::cos(angle), std::sin(angle)};
            Vec2 spawn_pos = pos.Value + dir * 0.5f;
            int arrow_id = ids.NextArrowId++;
            cb.push([arrow_id, spawn_pos, angle, net_id, owner, dmg](
                        entt::registry &r
                    ) {
                auto a = r.create();
                Vec2 vel{std::cos(angle), std::sin(angle)};
                vel *= GameConfig::ArrowSpeed;
                r.emplace<Position2D>(a, spawn_pos);
                r.emplace<Velocity2D>(a, vel);
                r.emplace<FacingAngle>(a, angle);
                r.emplace<ArrowTag>(a, net_id, owner, dmg, 1.0f);
                r.emplace<Lifetime>(a, GameConfig::ArrowLifetime);
                r.emplace<NetworkId>(a, arrow_id);
            });
        }
    }

    bool can_interrupt(CastState::Phase phase) const override {
        return phase == CastState::Phase::Chasing;
    }
};

} // namespace sim
