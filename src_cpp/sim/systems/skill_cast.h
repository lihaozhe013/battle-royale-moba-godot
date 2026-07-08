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
        NetworkId>();
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

        auto &cs = reg.get_or_emplace<CastState>(e);

        // Decay reject timer each tick
        if (cs.RejectTimer > 0.0f)
            cs.RejectTimer -= dt;

        int cast_slot = input.CastSlot;
        bool cast_confirm = input.CastConfirm;
        bool cast_cancel = input.CastCancel;
        bool cast_interrupt = input.CastInterrupt;
        Vec2 cast_aim = input.CastAim;
        bool moving = glm::length(input.Move) > 0.01f;

        switch (cs.State) {
        case CastState::Phase::None: {
            if (cast_slot >= 0 && cast_slot < 4 && cs.RejectTimer <= 0.0f) {
                auto &slot = skills.Slots[cast_slot];
                if (slot.SkillId > 0 && slot.CooldownTimer <= 0.0f &&
                    mana.Cur >= slot.ManaCost) {
                    cs.State = CastState::Phase::Aiming;
                    cs.ActiveSlot = cast_slot;
                    cs.EnteredSlot = cast_slot;
                    cs.RejectTimer = 0.15f;
                    cs.SkillId = slot.SkillId;
                    cs.Timer = 0.0f;
                    cs.SubTimer = 0.0f;
                    cs.AimPos = cast_aim;
                }
            }
            break;
        }

        case CastState::Phase::Aiming: {
            cs.AimPos = cast_aim;
            if (cast_confirm) {
                auto &slot = skills.Slots[cs.ActiveSlot];
                const auto &def = get_skill_def(cs.SkillId);
                mana.Cur -= slot.ManaCost;
                mana.RegenTimer = GameConfig::ManaRegenDelay;
                slot.CooldownTimer = slot.MaxCooldown;
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
                if (slot.SkillId > 0 && slot.CooldownTimer <= 0.0f &&
                    mana.Cur >= slot.ManaCost) {
                    cs.ActiveSlot = cast_slot;
                    cs.SkillId = slot.SkillId;
                    cs.AimPos = cast_aim;
                }
            }
            break;
        }

        case CastState::Phase::Casting: {
            if (cast_cancel || cast_interrupt) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1;
                cs.SkillId = 0;
                break;
            }
            cs.Timer -= dt;
            if (cs.Timer <= 0.0f) {
                const auto &def = get_skill_def(cs.SkillId);
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
                float dmg = stats.Atk;

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
        auto target_view = reg.view<Damageable, Position2D, Health>();
        entt::entity best_target = entt::null;
        float best_dist_sq = def.Range * def.Range;
        for (auto t : target_view) {
            if (t == caster_entity)
                continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(t).Value - cs.AimPos;
            float d_sq = vec2_length_sq(delta);
            if (d_sq < best_dist_sq) {
                best_dist_sq = d_sq;
                best_target = t;
            }
        }
        if (best_target != entt::null) {
            auto &hp = target_view.get<Health>(best_target);
            hp.Cur -= static_cast<int>(def.Damage);
            if (hp.Cur <= 0) {
                hp.Cur = 0;
                if (reg.all_of<Dead>(best_target))
                    reg.get<Dead>(best_target).enabled = true;
                if (reg.all_of<BotAIState>(best_target))
                    reg.get<BotAIState>(best_target).RespawnTimer =
                        GameConfig::BotRespawnTime;
                int victim_id = reg.all_of<NetworkId>(best_target)
                                    ? reg.get<NetworkId>(best_target).Value
                                    : 0;
                auto kill_view = reg.view<KillEventBuffer>();
                for (auto k : kill_view)
                    kill_view.get<KillEventBuffer>(k).events.push_back(
                        {net.Value, victim_id}
                    );
                if (reg.all_of<Kills>(caster_entity))
                    reg.get<Kills>(caster_entity).Value += 1;
            }
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
            hp.Cur -= static_cast<int>(def.Damage);
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
                st.Type = StatusType::Root;
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
