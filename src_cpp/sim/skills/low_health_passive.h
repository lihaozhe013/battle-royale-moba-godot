#pragma once

#include "../components.h"
#include "../game_config.h"
#include "skill_interface.h"
#include <entt/entt.hpp>

namespace sim {

class LowHealthPassiveSkill : public ISkill {
  public:
    explicit LowHealthPassiveSkill(const SkillTuning &tuning) : _tuning(tuning) {}

    int id() const override { return _tuning.Id; }
    SkillKind kind() const override { return SkillKind::LowHealthPassive; }
    SkillTargetMode target_mode() const override {
        return SkillTargetMode::Passive;
    }
    int max_level() const override { return 1; }
    bool is_passive() const override { return true; }

    float base_cooldown() const override { return 0.0f; }
    float base_mana_cost() const override { return 0.0f; }
    float base_cast_time() const override { return 0.0f; }
    float base_range(int) const override { return 0.0f; }

    int validate_cast(
        entt::registry &, entt::entity, const CastContext &
    ) override {
        return 4;
    }

    bool can_enter_casting(
        entt::registry &, entt::entity, const CastState &, int
    ) override {
        return false;
    }

    void on_cast_complete(
        entt::registry &, entt::entity, CastState &, CommandBuffer &, IdState &, int
    ) override {}

    void on_assigned(entt::registry &reg, entt::entity caster, int) override {
        auto &passive = reg.get_or_emplace<BasicAttackPassive>(caster);
        passive.LifestealMin = _tuning.LifestealMin;
        passive.LifestealMax = _tuning.LifestealMax;
        passive.CritChanceMin = _tuning.CritMin;
        passive.CritChanceMax = _tuning.CritMax;
        passive.CritMultiplier = _tuning.CritMultiplier;
    }

    void on_level_changed(entt::registry &reg, entt::entity caster, int level) override {
        on_assigned(reg, caster, level);
    }

  private:
    SkillTuning _tuning;
};

} // namespace sim
