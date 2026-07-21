#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../skills/skill_interface.h"
#include "../skills/skill_registry.h"
#include "../vec2.h"
#include "attack_command.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

inline void skill_cast_player_system(
    entt::registry &reg,
    entt::entity e,
    PlayerInputState &input,
    CastState &cs,
    SkillComponent &skills,
    Mana &mana,
    float dt,
    CommandBuffer &cb,
    IdState &ids
);

inline void skill_cast_aisystem(
    entt::registry &reg,
    entt::entity e,
    PlayerInputState &input,
    CastState &cs,
    SkillComponent &skills,
    Mana &mana,
    float dt,
    CommandBuffer &cb,
    IdState &ids
);

inline void skill_cast_system(
    entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids, double now
) {
    auto view = reg.view<
        PlayerTag,
        PlayerInputState,
        SkillComponent,
        Mana,
        Position2D,
        CombatStats,
        NetworkId,
        Level>();

    for (auto e : view) {
        auto &input = view.get<PlayerInputState>(e);
        auto &cs = reg.get_or_emplace<CastState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);

        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f) {
                if (input.SkillSlot >= 0)
                    cs.CastError = 3;
                continue;
            }
        }

        cs.HitTargetId = -1;

        bool is_bot = reg.all_of<BotTag>(e);
        if (is_bot) {
            skill_cast_aisystem(reg, e, input, cs, skills, mana, dt, cb, ids);
            continue;
        }

        skill_cast_player_system(reg, e, input, cs, skills, mana, dt, cb, ids);
    }
}

inline void skill_cast_player_system(
    entt::registry &reg,
    entt::entity e,
    PlayerInputState &input,
    CastState &cs,
    SkillComponent &skills,
    Mana &mana,
    float dt,
    CommandBuffer &cb,
    IdState &ids
) {
    auto &tag = reg.get<PlayerTag>(e);
    if (!tag.IsLocal)
        return;

    if (cs.RejectTimer > 0.0f)
        cs.RejectTimer -= dt;

    int cast_slot = input.SkillSlot;
    bool cast_confirm = input.SkillConfirm;
    bool cancel_skill = input.CancelSkill;
    Vec2 cast_aim = input.SkillAim;
    auto &lv = reg.get<Level>(e);

    auto refund_cast = [&](int slot_idx, int skill_id) {
        auto &s = skills.Slots[slot_idx];
        ISkill *sk = SkillRegistry::instance().get(skill_id);
        if (!sk)
            return;
        float mc = sk->mana_cost(s.Level);
        mana.Cur += mc;
        if (mana.Cur > mana.Max)
            mana.Cur = mana.Max;
        s.CooldownTimer = 0.0f;
    };

    switch (cs.State) {
    case CastState::Phase::None: {
        if (cast_slot < 0 || cast_slot >= 4 || cs.RejectTimer > 0.0f)
            break;
        auto &slot = skills.Slots[cast_slot];
        ISkill *sk = SkillRegistry::instance().get(slot.SkillId);
        if (!sk)
            break;

        if (!cast_confirm) {
            cs.ActiveSlot = cast_slot;
            cs.SkillId = slot.SkillId;
            cs.TargetEntity = resolve_target_by_netid(reg, input.SkillTargetId);
            cs.TargetNetworkId = input.SkillTargetId;
            cs.AimPos = cast_aim;
            break;
        }

        if (slot.CooldownTimer > 0.0f) {
            cs.CastError = 1;
            cs.RejectTimer = 0.3f;
            break;
        }

        CastContext ctx{
            e,
            slot,
            slot.Level,
            cast_aim,
            resolve_target_by_netid(reg, input.SkillTargetId),
            input.SkillTargetId,
            false
        };

        int err = sk->validate_cast(reg, e, ctx);
        if (err != 0) {
            cs.CastError = err;
            cs.RejectTimer = 0.3f;
            break;
        }

        float cd = sk->cooldown(slot.Level);
        float mc = sk->mana_cost(slot.Level);
        mana.Cur -= mc;
        mana.RegenTimer = GameConfig::ManaRegenDelay;
        slot.CooldownTimer = cd;
        slot.MaxCooldown = cd;

        cs.ActiveSlot = cast_slot;
        cs.SkillId = slot.SkillId;
        cs.TargetEntity = ctx.target_entity;
        cs.TargetNetworkId = ctx.target_network_id;
        cs.AimPos = ctx.aim_pos;

        if (sk->can_enter_casting(reg, e, cs, slot.Level)) {
            cs.State = CastState::Phase::Casting;
            cs.Timer = sk->cast_time(slot.Level);
        } else {
            cs.State = CastState::Phase::Chasing;
            cs.Timer = 0.0f;
        }
        cs.CastError = 0;
        break;
    }

    case CastState::Phase::Chasing: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (!sk) {
            cs.State = CastState::Phase::None;
            break;
        }

        if (cancel_skill && sk->can_interrupt(cs.State)) {
            if (GameConfig::RefundOnChaseInterrupt)
                refund_cast(cs.ActiveSlot, cs.SkillId);
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
            break;
        }

        sk->on_chase_tick(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);

        if (sk->can_enter_casting(
                reg, e, cs, skills.Slots[cs.ActiveSlot].Level
            )) {
            cs.State = CastState::Phase::Casting;
            cs.Timer = sk->cast_time(skills.Slots[cs.ActiveSlot].Level);
            cs.CastError = 0;
        }
        break;
    }

    case CastState::Phase::Casting: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (!sk) {
            cs.State = CastState::Phase::None;
            break;
        }

        if (cancel_skill && sk->can_interrupt(cs.State)) {
            if (GameConfig::RefundOnCastInterrupt)
                refund_cast(cs.ActiveSlot, cs.SkillId);
            cs.State = CastState::Phase::None;
            break;
        }

        cs.Timer -= dt;
        if (cs.Timer > 0.0f)
            break;

        sk->on_cast_complete(
            reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level
        );

        if (sk->kind() == SkillKind::MeleeSingle ||
            sk->kind() == SkillKind::AoEField) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        } else if (sk->kind() == SkillKind::Dash) {
            sk->on_dash_start(reg, e, cs, skills.Slots[cs.ActiveSlot].Level);
            cs.State = CastState::Phase::Dashing;
        } else if (sk->kind() == SkillKind::ChannelBurst) {
            cs.State = CastState::Phase::Channeling;
            cs.Timer = sk->effect_value(skills.Slots[cs.ActiveSlot].Level);
            cs.SubTimer = 0.0f;
        }
        break;
    }

    case CastState::Phase::Dashing: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (sk)
            sk->on_dash_update(
                reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt
            );
        Vec2 delta = cs.DashTarget - reg.get<Position2D>(e).Value;
        if (glm::length(delta) < 0.1f || cs.Timer <= 0.0f) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        }
        break;
    }

    case CastState::Phase::Channeling: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        cs.Timer -= dt;
        if (sk)
            sk->on_channel_tick(
                reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level, dt
            );
        if (cs.Timer <= 0.0f) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        }
        break;
    }
    case CastState::Phase::Aiming:
        break;
    }
}

inline void skill_cast_aisystem(
    entt::registry &reg,
    entt::entity e,
    PlayerInputState &input,
    CastState &cs,
    SkillComponent &skills,
    Mana &mana,
    float dt,
    CommandBuffer &cb,
    IdState &ids
) {
    switch (cs.State) {
    case CastState::Phase::None: {
        int cast_slot = input.SkillSlot;
        if (cast_slot < 0 || cast_slot >= 4)
            break;
        if (!input.SkillConfirm)
            break;

        auto &slot = skills.Slots[cast_slot];
        ISkill *sk = SkillRegistry::instance().get(slot.SkillId);
        if (!sk)
            break;
        if (slot.CooldownTimer > 0.0f)
            break;

        CastContext ctx{
            e,
            slot,
            slot.Level,
            input.SkillAim,
            resolve_target_by_netid(reg, input.SkillTargetId),
            input.SkillTargetId,
            false
        };

        int err = sk->validate_cast(reg, e, ctx);
        if (err != 0)
            break;

        float mc = sk->mana_cost(slot.Level);
        mc *= GameConfig::BotManaCostMul;
        mana.Cur -= mc;
        mana.RegenTimer = GameConfig::ManaRegenDelay;
        float cd = sk->cooldown(slot.Level);
        cd *= GameConfig::BotSkillCooldownMul;
        slot.CooldownTimer = cd;
        slot.MaxCooldown = cd;

        cs.ActiveSlot = cast_slot;
        cs.SkillId = slot.SkillId;
        cs.TargetEntity = ctx.target_entity;
        cs.TargetNetworkId = ctx.target_network_id;
        cs.AimPos = ctx.aim_pos;

        if (sk->can_enter_casting(reg, e, cs, slot.Level)) {
            cs.State = CastState::Phase::Casting;
            cs.Timer = sk->cast_time(slot.Level);
        } else {
            cs.State = CastState::Phase::Chasing;
            cs.Timer = 0.0f;
        }
        cs.CastError = 0;
        break;
    }

    case CastState::Phase::Chasing: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (!sk) {
            cs.State = CastState::Phase::None;
            break;
        }

        sk->on_chase_tick(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);

        if (sk->can_enter_casting(
                reg, e, cs, skills.Slots[cs.ActiveSlot].Level
            )) {
            cs.State = CastState::Phase::Casting;
            cs.Timer = sk->cast_time(skills.Slots[cs.ActiveSlot].Level);
            cs.CastError = 0;
        }
        break;
    }

    case CastState::Phase::Casting: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (!sk) {
            cs.State = CastState::Phase::None;
            break;
        }

        cs.Timer -= dt;
        if (cs.Timer > 0.0f)
            break;

        sk->on_cast_complete(
            reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level
        );

        if (sk->kind() == SkillKind::MeleeSingle ||
            sk->kind() == SkillKind::AoEField) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        } else if (sk->kind() == SkillKind::Dash) {
            sk->on_dash_start(reg, e, cs, skills.Slots[cs.ActiveSlot].Level);
            cs.State = CastState::Phase::Dashing;
        } else if (sk->kind() == SkillKind::ChannelBurst) {
            cs.State = CastState::Phase::Channeling;
            cs.Timer = sk->effect_value(skills.Slots[cs.ActiveSlot].Level);
            cs.SubTimer = 0.0f;
        }
        break;
    }

    case CastState::Phase::Dashing: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        if (sk)
            sk->on_dash_update(
                reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt
            );
        Vec2 delta = cs.DashTarget - reg.get<Position2D>(e).Value;
        if (glm::length(delta) < 0.1f || cs.Timer <= 0.0f) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        }
        break;
    }

    case CastState::Phase::Channeling: {
        ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
        cs.Timer -= dt;
        if (sk)
            sk->on_channel_tick(
                reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level, dt
            );
        if (cs.Timer <= 0.0f) {
            cs.State = CastState::Phase::None;
            cs.ActiveSlot = -1;
            cs.SkillId = 0;
        }
        break;
    }
    case CastState::Phase::Aiming:
        break;
    }
}

} // namespace sim
