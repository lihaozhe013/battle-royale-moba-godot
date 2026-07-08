#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include <entt/entt.hpp>
#include <limits>
#include <vector>

namespace sim {

inline void
bot_targeting_system(entt::registry &reg, std::mt19937 &rng, float dt) {
    auto bot_view = reg.view<BotTag, Position2D, BotVisionRange, BotAIState>();
    auto target_view = reg.view<Damageable, Position2D, Health>();

    for (auto bot : bot_view) {
        auto &ai = bot_view.get<BotAIState>(bot);
        auto &bot_pos = bot_view.get<Position2D>(bot);
        float vision = bot_view.get<BotVisionRange>(bot).Value;

        if (reg.all_of<Dead>(bot) && reg.get<Dead>(bot).enabled) {
            continue;
        }

        // ── Fix B: 目标锁定 ──
        bool current_valid = ai.TargetEntity != entt::null &&
                             reg.valid(ai.TargetEntity) &&
                             (!reg.all_of<Dead>(ai.TargetEntity) ||
                              !reg.get<Dead>(ai.TargetEntity).enabled);

        if (current_valid) {
            Vec2 delta =
                reg.get<Position2D>(ai.TargetEntity).Value - bot_pos.Value;
            float d_sq = vec2_length_sq(delta);
            if (d_sq <= vision * vision) {
                // 当前目标仍在视野内 → 锁定倒计时
                ai.TargetLockTimer -= dt;
                if (ai.TargetLockTimer > 0.0f) {
                    continue; // 锁定期内不换目标
                }
            }
        }
        ai.TargetLockTimer = 0.0f;

        struct Candidate {
            entt::entity entity;
            float dist_sq;
            int hp;
        };
        std::vector<Candidate> candidates;

        for (auto tgt : target_view) {
            if (tgt == bot)
                continue;
            if (reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled)
                continue;
            Vec2 delta = target_view.get<Position2D>(tgt).Value - bot_pos.Value;
            float d_sq = vec2_length_sq(delta);
            if (d_sq > vision * vision)
                continue;
            candidates.push_back({tgt, d_sq, target_view.get<Health>(tgt).Cur});
        }

        if (candidates.empty()) {
            ai.TargetEntity = entt::null;
            reg.replace<BotAIState>(bot, ai);
            continue;
        }

        // Select: min HP → min distance → random
        int min_hp = std::numeric_limits<int>::max();
        for (auto &c : candidates) {
            if (c.hp < min_hp)
                min_hp = c.hp;
        }

        std::vector<Candidate> hp_tier;
        for (auto &c : candidates) {
            if (c.hp == min_hp)
                hp_tier.push_back(c);
        }

        if (hp_tier.size() == 1) {
            ai.TargetEntity = hp_tier[0].entity;
            reg.replace<BotAIState>(bot, ai);
            continue;
        }

        float min_dist = std::numeric_limits<float>::max();
        for (auto &c : hp_tier) {
            if (c.dist_sq < min_dist)
                min_dist = c.dist_sq;
        }

        std::vector<Candidate> dist_tier;
        for (auto &c : hp_tier) {
            if (c.dist_sq == min_dist)
                dist_tier.push_back(c);
        }

        std::uniform_int_distribution<int> dist(0, (int)dist_tier.size() - 1);
        ai.TargetEntity = dist_tier[dist(rng)].entity;

        ai.TargetLockTimer = GameConfig::BotTargetLockTime;
        reg.replace<BotAIState>(bot, ai);
    }
}

} // namespace sim
