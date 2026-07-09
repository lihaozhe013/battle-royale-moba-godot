#pragma once

#include "../arrow_spawner.h"
#include "../command_buffer.h"
#include "../components.h"
#include "../game_config.h"
#include "../skill_defs.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

// ── Forward declarations ──
namespace detail {
inline void _trigger_effect(
    entt::registry &reg,
    CommandBuffer &cb,
    IdState &ids,
    double now,
    entt::entity caster_entity,
    CastState &cs,
    const SkillDef &def,
    Position2D &pos,
    CombatStats &stats,
    NetworkId &net
);
}

// ── Main system ──
inline void skill_cast_system(
    entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids, double now
) {
    // Process cast state machine for local player
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
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);
        auto &pos = view.get<Position2D>(e);
        auto &stats = view.get<CombatStats>(e);
        auto &net = view.get<NetworkId>(e);
        auto &lv = view.get<Level>(e);

        auto &cs = reg.get_or_emplace<CastState>(e);

        // Clear per-tick cast state fields
        cs.CastError = 0;
        cs.HitTargetId = -1;

        // Stun gate — 眩晕不能施法
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f) {
                if (input.CastSlot >= 0)
                    cs.CastError = 3;
                continue;
            }
        }

        // Decay reject timer each tick
        if (cs.RejectTimer > 0.0f)
            cs.RejectTimer -= dt;

        int cast_slot = input.CastSlot;
        bool cast_confirm = input.CastConfirm;
        bool cast_cancel = input.CastCancel;
        bool cast_interrupt = input.CastInterrupt;
        Vec2 cast_aim = input.CastAim;
        bool moving = glm::length(input.Move) > 0.01f;

        // ── Helper: resolve target from NetworkId ──
        auto resolve_target = [&](int net_id) -> entt::entity {
            if (net_id < 0) return entt::null;
            auto tv = reg.view<NetworkId, Damageable>();
            for (auto t : tv) {
                if (tv.get<NetworkId>(t).Value == net_id)
                    return t;
            }
            return entt::null;
        };

        // ── Helper: validate target alive + in range ──
        auto target_valid = [&](entt::entity t, const SkillDef &d) -> bool {
            if (!reg.valid(t)) return false;
            if (!reg.all_of<Damageable, Position2D, Health>(t)) return false;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled) return false;
            Vec2 delta = reg.get<Position2D>(t).Value - pos.Value;
            return vec2_length_sq(delta) <= d.Range * d.Range;
        };

        // ── Helper: refund mana and cooldown ──
        auto refund_cast = [&](int slot_idx, int skill_id) {
            auto &s = skills.Slots[slot_idx];
            const auto &rd = get_skill_def(skill_id);
            float rm = std::max(
                GameConfig::SkillManaReductionMin,
                1.0f - (lv.Value - 1) * rd.ManaReductionPerLevel
            );
            mana.Cur += rd.ManaCost * rm;
            if (mana.Cur > mana.Max) mana.Cur = mana.Max;
            s.CooldownTimer = 0.0f;
        };

        switch (cs.State) {
        case CastState::Phase::None: {
            if (cast_slot >= 0 && cast_slot < 4 && cs.RejectTimer <= 0.0f) {
                auto &slot = skills.Slots[cast_slot];
                if (!(slot.SkillId > 0)) break;
                const auto &mm_def = get_skill_def(slot.SkillId);
                float cd = slot.CooldownTimer;
                float mm_cost = mm_def.ManaCost * std::max(
                    GameConfig::SkillManaReductionMin,
                    1.0f - (lv.Value - 1) * mm_def.ManaReductionPerLevel
                );

                if (cd > 0.0f) {
                    cs.CastError = 1;
                    cs.RejectTimer = 0.15f;
                    break;
                }
                if (mana.Cur < mm_cost) {
                    cs.CastError = 2;
                    cs.RejectTimer = 0.15f;
                    break;
                }
                // For MeleeSingle, require a valid target
                entt::entity tgt = resolve_target(input.CastTargetId);
                if (mm_def.Kind == SkillKind::MeleeSingle) {
                    if (tgt == entt::null || !target_valid(tgt, mm_def)) {
                        cs.CastError = 4;
                        cs.RejectTimer = 0.15f;
                        break;
                    }
                }
                cs.State = CastState::Phase::Aiming;
                cs.ActiveSlot = cast_slot;
                cs.EnteredSlot = cast_slot;
                cs.RejectTimer = 0.15f;
                cs.SkillId = slot.SkillId;
                cs.TargetEntity = tgt;
                cs.Timer = 0.0f;
                cs.SubTimer = 0.0f;
                cs.AimPos = cast_aim;
            }
            break;
        }

        case CastState::Phase::Aiming: {
            cs.AimPos = cast_aim;
            if (cast_confirm) {
                auto &slot = skills.Slots[cs.ActiveSlot];
                const auto &def = get_skill_def(cs.SkillId);

                // Re-validate target for targeted skills
                if (def.Kind == SkillKind::MeleeSingle) {
                    if (!reg.valid(cs.TargetEntity) || !target_valid(cs.TargetEntity, def)) {
                        cs.State = CastState::Phase::None;
                        cs.ActiveSlot = -1;
                        cs.SkillId = 0;
                        cs.CastError = 4;
                        cs.RejectTimer = 0.3f;
                        break;
                    }
                }

                float cdr_mult = std::max(
                    GameConfig::SkillCDRMin,
                    1.0f - (lv.Value - 1) * GameConfig::SkillCDRPerLevel
                );
                float mana_mult = std::max(
                    GameConfig::SkillManaReductionMin,
                    1.0f - (lv.Value - 1) * def.ManaReductionPerLevel
                );
                float effective_cd = def.Cooldown * cdr_mult;
                float effective_mana = def.ManaCost * mana_mult;
                slot.MaxCooldown = effective_cd;
                mana.Cur -= effective_mana;
                mana.RegenTimer = GameConfig::ManaRegenDelay;
                slot.CooldownTimer = effective_cd;
                cs.AimPos = cast_aim;
                cs.Timer = def.CastTime;
                cs.State = CastState::Phase::Casting;
                break;
            }
            if (cast_cancel) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1;
                cs.SkillId = 0;
                break;
            }
            if (cast_slot >= 0 && cast_slot != cs.ActiveSlot) {
                auto &slot = skills.Slots[cast_slot];
                if (slot.SkillId > 0 && slot.CooldownTimer <= 0.0f) {
                    const auto &switch_def = get_skill_def(slot.SkillId);
                    float mm = std::max(
                        GameConfig::SkillManaReductionMin,
                        1.0f - (lv.Value - 1) * switch_def.ManaReductionPerLevel
                    );
                    if (mana.Cur >= switch_def.ManaCost * mm) {
                        cs.ActiveSlot = cast_slot;
                        cs.SkillId = slot.SkillId;
                        cs.AimPos = cast_aim;
                    }
                }
            }
            break;
        }

        case CastState::Phase::Casting: {
            if (cast_cancel || cast_interrupt) {
                refund_cast(cs.ActiveSlot, cs.SkillId);
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1;
                cs.SkillId = 0;
                break;
            }
            cs.Timer -= dt;
            if (cs.Timer <= 0.0f) {
                const auto &def = get_skill_def(cs.SkillId);

                // Validate target before triggering MeleeSingle effect
                if (def.Kind == SkillKind::MeleeSingle) {
                    if (!reg.valid(cs.TargetEntity) || !target_valid(cs.TargetEntity, def)) {
                        refund_cast(cs.ActiveSlot, cs.SkillId);
                        cs.State = CastState::Phase::None;
                        cs.ActiveSlot = -1;
                        cs.SkillId = 0;
                        cs.CastError = 5;
                        break;
                    }
                }

                detail::_trigger_effect(
                    reg, cb, ids, now, e, cs, def, pos, stats, net
                );
                switch (def.Kind) {
                case SkillKind::MeleeSingle:
                case SkillKind::AoEField:
                    cs.State = CastState::Phase::None;
                    cs.ActiveSlot = -1;
                    cs.SkillId = 0;
                    break;
                case SkillKind::Dash: {
                    Vec2 dir{1.0f, 0.0f};
                    if (glm::length(cs.AimPos - pos.Value) > 0.001f) {
                        dir = vec2_normalize(cs.AimPos - pos.Value);
                    }
                    cs.DashStart = pos.Value;
                    cs.DashTarget = pos.Value + dir * def.Range;
                    float dash_speed = 20.0f;
                    cs.Timer = def.Range / dash_speed;
                    cs.State = CastState::Phase::Dashing;
                    break;
                }
                case SkillKind::ChannelBurst:
                    cs.Timer = def.EffectValue;
                    cs.SubTimer = 0.0f;
                    cs.State = CastState::Phase::Channeling;
                    break;
                }
            }
            break;
        }

        case CastState::Phase::Dashing: {
            Vec2 delta = cs.DashTarget - pos.Value;
            float dist = glm::length(delta);
            if (dist < 0.1f || cs.Timer <= 0.0f) {
                pos.Value = cs.DashTarget;
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1;
                cs.SkillId = 0;
            } else {
                Vec2 dir = delta / dist;
                float speed = 20.0f;
                float step = std::min(dist, speed * dt);
                pos.Value = pos.Value + dir * step;
                cs.Timer -= dt;
            }
            break;
        }

        case CastState::Phase::Channeling: {
            cs.Timer -= dt;
            cs.SubTimer -= dt;

            if (cs.SubTimer <= 0.0f) {
                const auto &def = get_skill_def(cs.SkillId);
                float interval =
                    def.ChannelTick > 0.0f ? def.ChannelTick : 0.5f;
                cs.SubTimer = interval;
                int count = def.BulletCount > 0 ? def.BulletCount : 16;
                float dmg = def.Damage + stats.Atk * GameConfig::SkillDamageAtkRatio;

                for (int i = 0; i < count; ++i) {
                    float angle =
                        (float)i * (2.0f * 3.14159265f) / (float)count;
                    Vec2 dir{std::cos(angle), std::sin(angle)};
                    Vec2 spawn_pos = pos.Value + dir * 0.5f;

                    int arrow_id = ids.NextArrowId++;
                    cb.push([arrow_id,
                             spawn_pos,
                             angle,
                             net_id = net.Value,
                             owner = e,
                             dmg](entt::registry &r) {
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

            if (cs.Timer <= 0.0f) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1;
                cs.SkillId = 0;
            }
            break;
        }
        }
    }
}

// ── Effect trigger ──
namespace detail {

inline void _trigger_effect(
    entt::registry &reg,
    CommandBuffer &cb,
    IdState &ids,
    double now,
    entt::entity caster_entity,
    CastState &cs,
    const SkillDef &def,
    Position2D &pos,
    CombatStats &stats,
    NetworkId &net
) {
    switch (def.Kind) {
    case SkillKind::MeleeSingle: {
        entt::entity tgt = cs.TargetEntity;
        // Target is guaranteed valid by caller (Casting timer expiry check)
        auto &hp = reg.get<Health>(tgt);
        float skill_dmg = def.Damage + stats.Atk * GameConfig::SkillDamageAtkRatio;
        hp.Cur -= static_cast<int>(skill_dmg);
        if (reg.all_of<NetworkId>(tgt))
            cs.HitTargetId = reg.get<NetworkId>(tgt).Value;
        if (hp.Cur <= 0) {
            hp.Cur = 0;
            if (reg.all_of<Dead>(tgt))
                reg.get<Dead>(tgt).enabled = true;
            if (reg.all_of<BotAIState>(tgt))
                reg.get<BotAIState>(tgt).RespawnTimer =
                    GameConfig::BotRespawnTime;
            int victim_id = reg.all_of<NetworkId>(tgt)
                                ? reg.get<NetworkId>(tgt).Value
                                : 0;
            auto kill_view = reg.view<KillEventBuffer>();
            for (auto k : kill_view)
                kill_view.get<KillEventBuffer>(k).events.push_back(
                    {net.Value, victim_id}
                );
            if (reg.all_of<Kills>(caster_entity))
                reg.get<Kills>(caster_entity).Value += 1;
        }
        break;
    }

    case SkillKind::AoEField: {
        auto target_view = reg.view<Damageable, Position2D, Health>();
        for (auto t : target_view) {
            if (t == caster_entity)
                continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(t).Value - cs.AimPos;
            if (vec2_length_sq(delta) > def.Range * def.Range)
                continue;

            auto &hp = target_view.get<Health>(t);
            float skill_dmg = def.Damage + stats.Atk * GameConfig::SkillDamageAtkRatio;
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
                        {net.Value, victim_id}
                    );
                if (reg.all_of<Kills>(caster_entity))
                    reg.get<Kills>(caster_entity).Value += 1;
            } else {
                auto &st = reg.get_or_emplace<StatusEffect>(t);
                st.Type = StatusType::Stun;
                st.Timer = def.EffectValue;
            }
        }

        auto aoe = reg.create();
        reg.emplace<Position2D>(aoe, cs.AimPos);
        reg.emplace<AoETag>(
            aoe,
            net.Value,
            cs.SkillId,
            def.Range,
            def.EffectValue,
            def.EffectValue
        );
        reg.emplace<NetworkId>(aoe, ids.NextAoEId++);
        break;
    }

    case SkillKind::Dash:
    case SkillKind::ChannelBurst:
        break;
    }
}

} // namespace detail
} // namespace sim
