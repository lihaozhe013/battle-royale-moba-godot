#pragma once

#include "../command_buffer.h"
#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

struct CastContext {
    entt::entity caster;
    const SkillSlot &slot;
    int level;
    Vec2 aim_pos;
    entt::entity target_entity;
    int target_network_id;
    bool quick_cast;
};

class ISkill {
  public:
    virtual ~ISkill() = default;
    virtual int id() const = 0;
    virtual SkillKind kind() const = 0;

    virtual float base_cooldown() const { return 0.0f; }
    virtual float base_mana_cost() const { return 0.0f; }
    virtual float base_cast_time() const { return 0.0f; }
    virtual float base_range(int level) const { return 0.0f; }

    virtual float cooldown(int level) const { return base_cooldown(); }
    virtual float mana_cost(int level) const { return base_mana_cost(); }
    virtual float cast_time(int level) const { return base_cast_time(); }
    virtual float range(int level) const { return base_range(level); }
    virtual float damage(int level, float atk) const { return 0.0f; }
    virtual float effect_value(int level) const { return 0.0f; }

    virtual int validate_cast(
        entt::registry &reg, entt::entity caster, const CastContext &ctx
    ) = 0;

    virtual void on_cast_start(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        const CastContext &ctx
    ) {}

    virtual void on_chase_tick(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        int level,
        float dt
    ) {}

    virtual bool can_enter_casting(
        entt::registry &reg, entt::entity caster, const CastState &cs, int level
    ) = 0;

    virtual void on_cast_complete(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        int level
    ) = 0;

    virtual void on_channel_tick(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        CommandBuffer &cb,
        IdState &ids,
        int level,
        float dt
    ) {}

    virtual void on_dash_start(
        entt::registry &reg, entt::entity caster, CastState &cs, int level
    ) {}
    virtual void on_dash_update(
        entt::registry &reg,
        entt::entity caster,
        CastState &cs,
        int level,
        float dt
    ) {}

    virtual bool can_interrupt(CastState::Phase phase) const {
        return phase == CastState::Phase::Chasing ||
               phase == CastState::Phase::Casting;
    }
};

} // namespace sim
