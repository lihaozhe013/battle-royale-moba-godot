#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../skills/skill_interface.h"
#include "../skills/skill_registry.h"
#include "../vec2.h"
#include "attack_command.h"
#include "match_stats.h"
#include <algorithm>
#include <cstdio>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

struct CastContext;

namespace detail {

inline void clear_cast_state(CastState &cs) {
    cs.State = CastState::Phase::None;
    cs.ActiveSlot = -1;
    cs.SkillId = 0;
    cs.Timer = 0.0f;
    cs.SubTimer = 0.0f;
    cs.PendingCooldown = 0.0f;
    cs.PendingManaCost = 0.0f;
    cs.TargetEntity = entt::null;
    cs.TargetNetworkId = -1;
    cs.QuickCast = false;
}

inline void abort_cast_chase(
    entt::registry &reg,
    entt::entity e,
    CastState &cs,
    int error,
    const char *reason
) {
    bool is_local = reg.get<PlayerTag>(e).IsLocal;
    if (is_local) {
        cs.CastError = error;
        std::printf(
            "[skill_cast] chase_abort slot=%d skill=%d target=%d reason=%s\n",
            cs.ActiveSlot,
            cs.SkillId,
            cs.TargetNetworkId,
            reason
        );
    }
    if (reg.all_of<MovePath>(e))
        reg.get<MovePath>(e).Following = false;
    clear_cast_state(cs);
}

inline bool commit_cast_resources(
    entt::registry &reg, CastState &cs, SkillComponent &skills, Mana &mana
) {
    if (cs.ActiveSlot < 0 || cs.ActiveSlot >= 4)
        return false;
    if (cs.PendingManaCost > mana.Cur)
        return false;

    mana.Cur -= cs.PendingManaCost;
    mana.RegenTimer = stats(reg).ManaRegenDelay;
    auto &slot = skills.Slots[cs.ActiveSlot];
    slot.CooldownTimer = cs.PendingCooldown;
    slot.MaxCooldown = cs.PendingCooldown;
    return true;
}

inline void cast_phase_chasing(
    entt::registry &reg,
    entt::entity e,
    CastState &cs,
    SkillComponent &skills,
    bool cancel_skill,
    float dt
) {
    ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
    if (!sk) {
        clear_cast_state(cs);
        return;
    }

    if (cancel_skill && sk->can_interrupt(cs.State)) {
        if (reg.all_of<MovePath>(e))
            reg.get<MovePath>(e).Following = false;
        clear_cast_state(cs);
        return;
    }

    if (sk->target_mode() == SkillTargetMode::Unit) {
        bool target_invalid = cs.TargetEntity == entt::null ||
                              !reg.valid(cs.TargetEntity) ||
                              !reg.all_of<Position2D>(cs.TargetEntity);
        bool target_dead = !target_invalid &&
                           reg.all_of<Dead>(cs.TargetEntity) &&
                           reg.get<Dead>(cs.TargetEntity).enabled;
        if (target_invalid || target_dead) {
            abort_cast_chase(
                reg, e, cs, 5, target_dead ? "target_dead" : "target_invalid"
            );
            return;
        }
    }

    sk->on_chase_tick(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);

    bool in_range =
        sk->can_enter_casting(reg, e, cs, skills.Slots[cs.ActiveSlot].Level);
    if (in_range) {
        cs.State = CastState::Phase::Casting;
        cs.Timer = sk->cast_time(skills.Slots[cs.ActiveSlot].Level);
        cs.CastError = 0;
        return;
    }

    bool has_path = reg.all_of<MovePath>(e) && reg.get<MovePath>(e).Following;
    if (!has_path && reg.all_of<PathQueryState>(e)) {
        const auto &query = reg.get<PathQueryState>(e);
        // An asynchronous navigation request intentionally has no path until
        // a later tick. Keep the chase phase alive for the entire active
        // skill intent so the navigation phase can submit/repath after a
        // request completes or a waypoint is consumed.
        has_path = query.HasIntent &&
                   query.Channel == PathQueryChannel::Skill;
    }
    if (!has_path) {
        abort_cast_chase(reg, e, cs, 5, "no_path");
    }
}

inline void cast_phase_casting(
    entt::registry &reg,
    entt::entity e,
    CastState &cs,
    SkillComponent &skills,
    bool cancel_skill,
    float dt,
    CommandBuffer &cb,
    IdState &ids
) {
    ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
    if (!sk) {
        clear_cast_state(cs);
        return;
    }

    if (cancel_skill && sk->can_interrupt(cs.State)) {
        clear_cast_state(cs);
        return;
    }

    cs.Timer -= dt;
    if (cs.Timer > 0.0f)
        return;

    if (sk->target_mode() == SkillTargetMode::Unit) {
        bool invalid_target =
            cs.TargetEntity == entt::null || !reg.valid(cs.TargetEntity) ||
            !reg.all_of<Position2D>(cs.TargetEntity) ||
            (reg.all_of<Dead>(cs.TargetEntity) &&
             reg.get<Dead>(cs.TargetEntity).enabled);
        if (!invalid_target) {
            const auto &slot = skills.Slots[cs.ActiveSlot];
            CastContext target_ctx{
                e,
                slot,
                slot.Level,
                cs.AimPos,
                cs.TargetEntity,
                cs.TargetNetworkId,
                cs.QuickCast,
            };
            invalid_target = sk->validate_cast(reg, e, target_ctx) != 0;
            if (!invalid_target &&
                !sk->can_enter_casting(reg, e, cs, slot.Level))
                invalid_target = true;
        }
        if (invalid_target) {
            clear_cast_state(cs);
            if (reg.all_of<MovePath>(e))
                reg.get<MovePath>(e).Following = false;
            return;
        }
    }

    if (!commit_cast_resources(reg, cs, skills, reg.get<Mana>(e))) {
        clear_cast_state(cs);
        return;
    }
    record_skill_cast(reg, e);

    sk->on_cast_complete(
        reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level
    );

    if (sk->kind() == SkillKind::MeleeSingle ||
        sk->kind() == SkillKind::AoEField ||
        sk->kind() == SkillKind::TerrainRush ||
        sk->kind() == SkillKind::TargetTeleport ||
        sk->kind() == SkillKind::RadialSlow) {
        clear_cast_state(cs);
    } else if (sk->kind() == SkillKind::Dash) {
        sk->on_dash_start(reg, e, cs, skills.Slots[cs.ActiveSlot].Level);
        cs.State = CastState::Phase::Dashing;
    } else if (sk->kind() == SkillKind::ChannelBurst) {
        cs.State = CastState::Phase::Channeling;
        cs.Timer = sk->effect_value(skills.Slots[cs.ActiveSlot].Level);
        cs.SubTimer = 0.0f;
    }
}

inline void cast_phase_dashing(
    entt::registry &reg,
    entt::entity e,
    CastState &cs,
    SkillComponent &skills,
    float dt
) {
    ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
    if (sk)
        sk->on_dash_update(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);
    Vec2 delta = cs.DashTarget - reg.get<Position2D>(e).Value;
    if (glm::length(delta) < 0.1f || cs.Timer <= 0.0f) {
        clear_cast_state(cs);
    }
}

inline void cast_phase_channeling(
    entt::registry &reg,
    entt::entity e,
    CastState &cs,
    SkillComponent &skills,
    float dt,
    CommandBuffer &cb,
    IdState &ids
) {
    ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
    cs.Timer -= dt;
    if (sk)
        sk->on_channel_tick(
            reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level, dt
        );
    if (cs.Timer <= 0.0f) {
        clear_cast_state(cs);
    }
}

} // namespace detail

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
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) {
            if (reg.all_of<CastState>(e)) {
                auto &cs = reg.get<CastState>(e);
                detail::clear_cast_state(cs);
            }
            continue;
        }

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
        bool is_local = reg.get<PlayerTag>(e).IsLocal;

        if (cs.State == CastState::Phase::None) {
            if (is_local && cs.RejectTimer > 0.0f) {
                cs.RejectTimer = std::max(0.0f, cs.RejectTimer - dt);
                if (cs.RejectTimer > 0.0f)
                    continue;
            }

            int cast_slot = input.SkillSlot;
            if (cast_slot < 0 || cast_slot >= 4)
                continue;

            auto &slot = skills.Slots[cast_slot];
            ISkill *sk = SkillRegistry::instance().get(slot.SkillId);
            if (!sk)
                continue;

            if (sk->is_passive())
                continue;

            if (!input.SkillConfirm)
                continue;

            if (is_local) {
                if (slot.CooldownTimer > 0.0f) {
                    cs.CastError = 1;
                    cs.RejectTimer = 0.3f;
                    continue;
                }
            } else {
                if (slot.CooldownTimer > 0.0f)
                    continue;
            }

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
            if (err != 0) {
                if (is_local) {
                    cs.CastError = err;
                    cs.RejectTimer = 0.3f;
                }
                continue;
            }

            float cd = sk->cooldown(slot.Level);
            float mc = sk->mana_cost(slot.Level);
            if (is_bot) {
                mc *= stats(reg).BotManaCostMul;
                cd *= stats(reg).BotSkillCooldownMul;
            }
            if (mana.Cur < mc) {
                if (is_local) {
                    cs.CastError = 2;
                    cs.RejectTimer = 0.3f;
                }
                continue;
            }

            cs.ActiveSlot = cast_slot;
            cs.SkillId = slot.SkillId;
            cs.TargetEntity = ctx.target_entity;
            cs.TargetNetworkId = ctx.target_network_id;
            cs.AimPos = ctx.aim_pos;
            cs.PendingCooldown = cd;
            cs.PendingManaCost = mc;

            if (sk->can_enter_casting(reg, e, cs, slot.Level)) {
                cs.State = CastState::Phase::Casting;
                cs.Timer = sk->cast_time(slot.Level);
                if (cs.Timer <= 0.0f) {
                    detail::cast_phase_casting(
                        reg,
                        e,
                        cs,
                        skills,
                        false,
                        0.0f,
                        cb,
                        ids
                    );
                }
            } else {
                // A skill chase owns the movement path. Drop any previous
                // command first so an unreachable target cancels without
                // inheriting stale waypoints from ordinary movement.
                if (reg.all_of<MovePath>(e))
                    reg.get<MovePath>(e).Following = false;
                cs.State = CastState::Phase::Chasing;
                cs.Timer = 0.0f;
            }
            cs.CastError = 0;
            continue;
        }

        switch (cs.State) {
        case CastState::Phase::Chasing:
            detail::cast_phase_chasing(
                reg, e, cs, skills, input.CancelSkill, dt
            );
            break;
        case CastState::Phase::Casting:
            detail::cast_phase_casting(
                reg, e, cs, skills, input.CancelSkill, dt, cb, ids
            );
            break;
        case CastState::Phase::Dashing:
            detail::cast_phase_dashing(reg, e, cs, skills, dt);
            break;
        case CastState::Phase::Channeling:
            detail::cast_phase_channeling(reg, e, cs, skills, dt, cb, ids);
            break;
        case CastState::Phase::Aiming:
        case CastState::Phase::None:
            break;
        }
    }
}

} // namespace sim
