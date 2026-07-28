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

        bool current_valid = ai.TargetEntity != entt::null &&
                             reg.valid(ai.TargetEntity) &&
                             (!reg.all_of<Dead>(ai.TargetEntity) ||
                              !reg.get<Dead>(ai.TargetEntity).enabled);

        if (current_valid) {
            Vec2 delta =
                reg.get<Position2D>(ai.TargetEntity).Value - bot_pos.Value;
            float d_sq = vec2_length_sq(delta);
            if (d_sq <= vision * vision) {
                ai.TargetLockTimer -= dt;
                if (ai.TargetLockTimer > 0.0f) {
                    continue;
                }
            }
        }
        ai.TargetLockTimer = 0.0f;

        struct Candidate {
            entt::entity entity;
            float dist_sq;
            int hp;
            bool is_player;
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
            bool is_player =
                reg.all_of<HeroTag>(tgt) && reg.get<HeroTag>(tgt).IsLocal;
            candidates.push_back(
                {tgt, d_sq, target_view.get<Health>(tgt).Cur, is_player}
            );
        }

        if (candidates.empty()) {
            ai.TargetEntity = entt::null;
            reg.replace<BotAIState>(bot, ai);
            continue;
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const Candidate &a, const Candidate &b) {
                if (a.is_player != b.is_player)
                    return a.is_player > b.is_player;
                if (a.hp != b.hp)
                    return a.hp < b.hp;
                return a.dist_sq < b.dist_sq;
            }
        );

        ai.TargetEntity = candidates[0].entity;
        ai.TargetLockTimer = GameConfig::BotTargetLockTime;
        reg.replace<BotAIState>(bot, ai);
    }
}

} // namespace sim