#pragma once

#include "../game_config.h"
#include "../vec2.h"
#include "skill_interface.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

class DashSkill : public ISkill {
  public:
    int id() const override { return 3; }
    SkillKind kind() const override { return SkillKind::Dash; }

    float base_cooldown() const override { return 10.0f; }
    float base_mana_cost() const override { return 40.0f; }
    float base_cast_time() const override { return 0.2f; }
    float base_range(int) const override { return 8.0f; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * 1.0f;
    }
    float mana_cost(int level) const override {
        return base_mana_cost() * std::max(0.3f, 1.0f - (level - 1) * 0.05f);
    }
    float range(int level) const override {
        return base_range(level) + (level - 1) * 1.0f;
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
        Vec2 dir{1.0f, 0.0f};
        auto &pos = reg.get<Position2D>(caster);
        if (glm::length(cs.AimPos - pos.Value) > 0.001f)
            dir = vec2_normalize(cs.AimPos - pos.Value);
        cs.DashStart = pos.Value;
        cs.DashTarget = pos.Value + dir * range(level);
        cs.Timer = range(level) / 20.0f;
    }

    bool can_interrupt(CastState::Phase phase) const override { return false; }
};

} // namespace sim
