#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../skills/skill_interface.h"
#include "../skills/skill_registry.h"
#include "../vec2.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <random>
#include <vector>

namespace sim {

struct SkillScore {
    int slot;
    float score;
};

inline bool bot_skill_ready(
    const SkillSlot &s, const Mana &m, int level, const ISkill *sk
) {
    if (!sk)
        return false;
    if (s.CooldownTimer > 0.0f)
        return false;
    float effective_cost = sk->mana_cost(level) * GameConfig::BotManaCostMul;
    if (m.Cur < effective_cost)
        return false;
    return true;
}

inline float calculate_skill_score(
    const ISkill *sk,
    const SkillSlot &slot,
    float dist,
    float hp_ratio,
    int enemy_count,
    BotCombatState::Phase phase
) {
    if (!sk)
        return 0.0f;

    float score = 0.0f;
    float range = sk->range(slot.Level);

    score += 50.0f;

    if (dist <= range)
        score += 30.0f;
    else if (dist <= range * 1.5f)
        score += 10.0f;
    else
        score -= 20.0f;

    switch (phase) {
    case BotCombatState::Phase::Approach:
        if (sk->kind() == SkillKind::Dash)
            score += 40.0f;
        if (sk->kind() == SkillKind::MeleeSingle)
            score += 20.0f;
        break;
    case BotCombatState::Phase::Kite:
        if (sk->kind() == SkillKind::AoEField)
            score += 30.0f;
        if (sk->kind() == SkillKind::ChannelBurst)
            score += 25.0f;
        break;
    case BotCombatState::Phase::Burst:
        if (sk->kind() == SkillKind::MeleeSingle)
            score += 50.0f;
        if (sk->kind() == SkillKind::AoEField)
            score += 40.0f;
        break;
    case BotCombatState::Phase::Sustain:
        if (sk->kind() == SkillKind::ChannelBurst)
            score += 45.0f;
        if (sk->kind() == SkillKind::AoEField)
            score += 20.0f;
        break;
    case BotCombatState::Phase::Disengage:
        if (sk->kind() == SkillKind::Dash)
            score += 60.0f;
        break;
    }

    if (hp_ratio < 0.3f && sk->kind() == SkillKind::Dash)
        score += 50.0f;
    if (hp_ratio > 0.7f && sk->kind() == SkillKind::ChannelBurst)
        score += 20.0f;
    if (hp_ratio < 0.5f && sk->kind() == SkillKind::ChannelBurst)
        score -= 30.0f;

    if (sk->kind() == SkillKind::AoEField && enemy_count >= 2) {
        score += enemy_count * 25.0f;
    }

    return score;
}

inline void bot_skill_decider_system(entt::registry &reg, std::mt19937 &rng) {
    auto view = reg.view<
        BotTag,
        BotBehaviorState,
        BotAIState,
        BotCombatState,
        SkillComponent,
        Mana,
        Position2D,
        Level,
        Health>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
            continue;
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage)
            continue;

        auto &ai = view.get<BotAIState>(e);
        auto &combat = view.get<BotCombatState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);
        auto &pos = view.get<Position2D>(e);
        auto &hp = view.get<Health>(e);
        auto &lv = view.get<Level>(e);

        auto &rq = reg.get_or_emplace<BotCastRequest>(e);
        rq.Valid = false;
        rq.Score = 0.0f;

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity))
            continue;
        if (!reg.all_of<Position2D>(ai.TargetEntity))
            continue;

        bool target_alive =
            !(reg.all_of<Dead>(ai.TargetEntity) &&
              reg.get<Dead>(ai.TargetEntity).enabled);
        if (!target_alive)
            continue;

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        float hp_ratio = (float)hp.Cur / (float)hp.Max;

        const ISkill *sk[4] = {
            SkillRegistry::instance().get(skills.Slots[0].SkillId),
            SkillRegistry::instance().get(skills.Slots[1].SkillId),
            SkillRegistry::instance().get(skills.Slots[2].SkillId),
            SkillRegistry::instance().get(skills.Slots[3].SkillId),
        };

        int enemy_count = 0;
        auto damageable_view = reg.view<Damageable, Position2D>();
        for (auto t : damageable_view) {
            if (t == e)
                continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                continue;
            Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
            if (vec2_length_sq(d) <= 100.0f) {
                enemy_count++;
            }
        }

        std::vector<SkillScore> candidates;
        for (int i = 0; i < 4; ++i) {
            if (!sk[i])
                continue;
            if (!bot_skill_ready(
                    skills.Slots[i], mana, skills.Slots[i].Level, sk[i]
                ))
                continue;

            float score = calculate_skill_score(
                sk[i],
                skills.Slots[i],
                dist,
                hp_ratio,
                enemy_count,
                combat.Current
            );
            candidates.push_back({i, score});
        }

        if (candidates.empty())
            continue;

        std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b) {
            return a.score > b.score;
        });

        auto &best = candidates[0];
        rq.TargetSlot = best.slot;
        rq.Score = best.score;
        rq.Valid = true;

        if (sk[best.slot]->kind() == SkillKind::MeleeSingle) {
            rq.TargetNetworkId = reg.all_of<NetworkId>(ai.TargetEntity)
                                     ? reg.get<NetworkId>(ai.TargetEntity).Value
                                     : -1;
            rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
        } else if (sk[best.slot]->kind() == SkillKind::Dash) {
            Vec2 away_dir = (dist > 0.001f) ? -(to_target / dist) : Vec2{1, 0};
            if (hp_ratio < 0.3f) {
                rq.AimPos =
                    pos.Value +
                    away_dir *
                        sk[best.slot]->range(skills.Slots[best.slot].Level);
            } else {
                rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
            }
            rq.TargetNetworkId = -1;
        } else if (sk[best.slot]->kind() == SkillKind::AoEField) {
            Vec2 sum_pos{0, 0};
            int count = 0;
            for (auto t : damageable_view) {
                if (t == e)
                    continue;
                if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled)
                    continue;
                Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
                if (vec2_length_sq(d) <=
                    sk[best.slot]->range(skills.Slots[best.slot].Level) *
                        sk[best.slot]->range(skills.Slots[best.slot].Level)) {
                    sum_pos =
                        sum_pos + damageable_view.get<Position2D>(t).Value;
                    count++;
                }
            }
            rq.AimPos = (count > 0) ? sum_pos / (float)count : pos.Value;
            rq.TargetNetworkId = -1;
        } else {
            rq.AimPos = {0, 0};
            rq.TargetNetworkId = -1;
        }
    }
}

} // namespace sim