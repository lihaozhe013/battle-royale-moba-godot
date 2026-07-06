#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"

namespace sim {

inline void bot_ai_system(entt::registry &reg, float dt, float map_half, std::mt19937 &rng) {
    auto view = reg.view<BotTag, Position2D, FacingAngle, BotAIState, MoveSpeed, Health>();
    for (auto e : view) {
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &speed = view.get<MoveSpeed>(e);
        auto &hp = view.get<Health>(e);

        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;

        if (dead) {
            ai.RespawnTimer -= dt;
            if (ai.RespawnTimer <= 0.0f) {
                reg.get<Dead>(e).enabled = false;
                float half = map_half - GameConfig::BotRadius;
                pos.Value = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                hp.Cur = GameConfig::BotHp;
                ai.MoveTarget = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.RespawnTimer = 0.0f;
                ai.TargetEntity = entt::null;
                ai.WanderTimer = GameConfig::BotWanderIntervalMin
                    + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)
                    * (GameConfig::BotWanderIntervalMax - GameConfig::BotWanderIntervalMin);
            }
            continue;
        }

        // Wander
        ai.WanderTimer -= dt;
        if (ai.WanderTimer <= 0.0f) {
            Vec2 diff = ai.MoveTarget - pos.Value;
            if (vec2_length_sq(diff) < 0.25f) {
                float half = map_half - GameConfig::BotRadius;
                ai.MoveTarget = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.WanderTimer = GameConfig::BotWanderIntervalMin
                    + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)
                    * (GameConfig::BotWanderIntervalMax - GameConfig::BotWanderIntervalMin);
            }
        }

        Vec2 wander_dir = ai.MoveTarget - pos.Value;
        float dist = glm::length(wander_dir);
        if (dist > 0.01f) {
            Vec2 step = (wander_dir / dist) * speed.Value * dt;
            float move_dist = std::min(dist, glm::length(step));
            pos.Value = vec2_clamp_to_map(
                pos.Value + (wander_dir / dist) * move_dist, map_half);
        }

        // Face movement direction (mirrors player_movement_system)
        if (dist > 0.01f) {
            angle.Radians = std::atan2(wander_dir.y, wander_dir.x);
        }
    }
}

} // namespace sim
