#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../skills/skill_interface.h"
#include "../skills/skill_registry.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace sim {

inline bool has_dash_ready(entt::registry &reg, entt::entity e) {
    auto &skills = reg.get<SkillComponent>(e);
    auto &mana = reg.get<Mana>(e);
    const ISkill *sk = SkillRegistry::instance().get(skills.Slots[2].SkillId);
    if (!sk)
        return false;
    if (skills.Slots[2].CooldownTimer > 0.0f)
        return false;
    float effective_cost =
        sk->mana_cost(skills.Slots[2].Level) * GameConfig::BotManaCostMul;
    return mana.Cur >= effective_cost;
}

inline bool has_burst_skills_ready(entt::registry &reg, entt::entity e) {
    auto &skills = reg.get<SkillComponent>(e);
    auto &mana = reg.get<Mana>(e);

    for (int i = 0; i < 4; ++i) {
        const ISkill *sk =
            SkillRegistry::instance().get(skills.Slots[i].SkillId);
        if (!sk)
            continue;
        if (skills.Slots[i].CooldownTimer > 0.0f)
            continue;
        float effective_cost =
            sk->mana_cost(skills.Slots[i].Level) * GameConfig::BotManaCostMul;
        if (mana.Cur < effective_cost)
            continue;
        if (sk->kind() == SkillKind::MeleeSingle ||
            sk->kind() == SkillKind::AoEField) {
            return true;
        }
    }
    return false;
}

inline void bot_combat_state_system(entt::registry &reg, float dt) {
    auto view = reg.view<
        BotTag,
        BotCombatState,
        BotBehaviorState,
        BotAIState,
        Health,
        Position2D,
        CombatStats>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
            continue;
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage) {
            auto &combat = view.get<BotCombatState>(e);
            combat.Current = BotCombatState::Phase::Approach;
            continue;
        }

        auto &combat = view.get<BotCombatState>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &hp = view.get<Health>(e);
        auto &pos = view.get<Position2D>(e);
        auto &stats = view.get<CombatStats>(e);

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity))
            continue;
        if (!reg.all_of<Position2D>(ai.TargetEntity))
            continue;

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        float hp_ratio = (float)hp.Cur / (float)hp.Max;
        float attack_range = GameConfig::PlayerAttackRange;

        combat.PhaseTimer -= dt;
        if (combat.PhaseTimer > 0.0f)
            continue;

        BotCombatState::Phase new_phase = combat.Current;
        bool changed = false;

        switch (combat.Current) {
        case BotCombatState::Phase::Approach:
            if (dist < attack_range * GameConfig::BotApproachThreshold) {
                new_phase = BotCombatState::Phase::Kite;
                changed = true;
            } else if (
                hp_ratio < GameConfig::BotDisengageHealthThreshold &&
                has_dash_ready(reg, e)
            ) {
                new_phase = BotCombatState::Phase::Disengage;
                changed = true;
            }
            break;

        case BotCombatState::Phase::Kite:
            if (dist > attack_range * GameConfig::BotKiteThreshold) {
                new_phase = BotCombatState::Phase::Approach;
                changed = true;
            } else if (
                hp_ratio > GameConfig::BotBurstHealthThreshold &&
                has_burst_skills_ready(reg, e)
            ) {
                new_phase = BotCombatState::Phase::Burst;
                changed = true;
            } else if (hp_ratio < GameConfig::BotSustainHealthThreshold) {
                new_phase = BotCombatState::Phase::Disengage;
                changed = true;
            }
            break;

        case BotCombatState::Phase::Burst:
            if (combat.BurstStep >= 3 || combat.BurstTimer <= 0.0f) {
                new_phase = BotCombatState::Phase::Sustain;
                changed = true;
            }
            break;

        case BotCombatState::Phase::Sustain:
            if (hp_ratio < GameConfig::BotSustainHealthThreshold) {
                new_phase = BotCombatState::Phase::Disengage;
                changed = true;
            } else if (dist > attack_range * GameConfig::BotKiteThreshold) {
                new_phase = BotCombatState::Phase::Approach;
                changed = true;
            }
            break;

        case BotCombatState::Phase::Disengage:
            if (dist > attack_range * GameConfig::BotSafeDistanceMultiplier ||
                hp_ratio > 0.6f) {
                new_phase = BotCombatState::Phase::Approach;
                changed = true;
            }
            break;
        }

        if (changed) {
            combat.Current = new_phase;
            combat.PhaseTimer = GameConfig::BotCombatPhaseCooldown;
            if (new_phase == BotCombatState::Phase::Burst) {
                combat.BurstStep = 0;
                combat.BurstTimer = 2.0f;
            }
        }

        if (combat.Current == BotCombatState::Phase::Burst) {
            combat.BurstTimer -= dt;
        }
    }
}

} // namespace sim