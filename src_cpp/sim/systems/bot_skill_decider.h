#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../skills/skill_registry.h"
#include "../skills/skill_interface.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <random>

namespace sim {

inline bool bot_skill_ready(const SkillSlot &s, const Mana &m, int level,
                             const ISkill *sk) {
    if (!sk) return false;
    if (s.CooldownTimer > 0.0f) return false;
    float effective_cost = sk->mana_cost(level) * GameConfig::BotManaCostMul;
    if (m.Cur < effective_cost) return false;
    return true;
}

inline bool bot_target_in_range(entt::registry &reg, entt::entity caster,
                                 entt::entity target, const ISkill *sk, int level) {
    if (!reg.valid(target)) return false;
    Vec2 delta = reg.get<Position2D>(target).Value -
                 reg.get<Position2D>(caster).Value;
    return vec2_length_sq(delta) <= sk->range(level) * sk->range(level);
}

inline void bot_skill_decider_system(entt::registry &reg, std::mt19937 &rng) {
    auto view = reg.view<
        BotTag, BotBehaviorState, BotAIState, SkillComponent,
        Mana, Position2D, Level>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage) continue;

        auto &ai = view.get<BotAIState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);
        auto &pos = view.get<Position2D>(e);
        auto &lv = view.get<Level>(e);

        auto &rq = reg.get_or_emplace<BotCastRequest>(e);
        rq.Valid = false;

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity)) continue;

        bool target_alive = !(reg.all_of<Dead>(ai.TargetEntity) &&
                              reg.get<Dead>(ai.TargetEntity).enabled);
        if (!target_alive) continue;

        const ISkill *sk[4] = {
            SkillRegistry::instance().get(skills.Slots[0].SkillId),
            SkillRegistry::instance().get(skills.Slots[1].SkillId),
            SkillRegistry::instance().get(skills.Slots[2].SkillId),
            SkillRegistry::instance().get(skills.Slots[3].SkillId),
        };

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        Vec2 away_dir = (dist > 0.001f) ? -(to_target / dist) : Vec2{1,0};

        float hp_ratio = (float)reg.get<Health>(e).Cur /
                         (float)reg.get<Health>(e).Max;

        float effective_range[4];
        for (int i = 0; i < 4; ++i) {
            effective_range[i] = sk[i] ? sk[i]->range(skills.Slots[i].Level) : 0.0f;
        }

        // ── P1: Dash 逃生 ──
        if (hp_ratio < 0.3f && dist < effective_range[2] * 1.5f) {
            if (bot_skill_ready(skills.Slots[2], mana, skills.Slots[2].Level, sk[2])) {
                rq.TargetSlot = 2;
                rq.AimPos = pos.Value + away_dir * effective_range[2];
                rq.TargetNetworkId = -1;
                rq.Valid = true;
                continue;
            }
        }

        // ── P2: AoE 群控 ──
        if (sk[1] && bot_skill_ready(skills.Slots[1], mana, skills.Slots[1].Level, sk[1])) {
            int enemy_count = 0;
            Vec2 sum_pos{0,0};
            auto damageable_view = reg.view<Damageable, Position2D>();
            for (auto t : damageable_view) {
                if (t == e) continue;
                if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled) continue;
                Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
                if (vec2_length_sq(d) <= effective_range[1] * effective_range[1]) {
                    enemy_count++;
                    sum_pos = sum_pos + damageable_view.get<Position2D>(t).Value;
                }
            }
            if (enemy_count >= 2) {
                rq.TargetSlot = 1;
                rq.AimPos = sum_pos / (float)enemy_count;
                rq.TargetNetworkId = -1;
                rq.Valid = true;
                continue;
            }
        }

        // ── P3: MeleeSingle 爆发 ──
        if (sk[0] && bot_skill_ready(skills.Slots[0], mana, skills.Slots[0].Level, sk[0])) {
            int target_net = reg.all_of<NetworkId>(ai.TargetEntity)
                ? reg.get<NetworkId>(ai.TargetEntity).Value : -1;
            rq.TargetSlot = 0;
            rq.TargetNetworkId = target_net;
            rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
            rq.Valid = true;
            continue;
        }

        // ── P4: ChannelBurst 持续 ──
        if (hp_ratio > 0.5f && sk[3] &&
            bot_skill_ready(skills.Slots[3], mana, skills.Slots[3].Level, sk[3]) &&
            dist < effective_range[3] * 1.2f) {
            rq.TargetSlot = 3;
            rq.TargetNetworkId = -1;
            rq.AimPos = {0,0};
            rq.Valid = true;
            continue;
        }

        rq.Valid = false;
    }
}

} // namespace sim
