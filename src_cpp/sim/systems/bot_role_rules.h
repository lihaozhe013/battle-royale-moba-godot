#pragma once

#include "../components.h"
#include "../game_config.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <random>

namespace sim {

namespace detail {

inline int
roll_bot_level_for_role(entt::registry &reg, BotRole role, std::mt19937 &rng) {
    switch (role) {
    case BotRole::Fodder:
        return std::uniform_int_distribution<int>(1, GameConfig::FodderMaxLv)(
            rng
        );
    case BotRole::Brute:
        return std::uniform_int_distribution<
            int>(GameConfig::BruteMinLv, GameConfig::MaxBotLevel)(rng);
    case BotRole::Stalker: {
        int plv = 1;
        auto pv = reg.view<PlayerTag, Level>();
        for (auto p : pv) {
            if (pv.get<PlayerTag>(p).IsLocal) {
                plv = pv.get<Level>(p).Value;
                break;
            }
        }
        int off = std::uniform_int_distribution<
            int>(-GameConfig::StalkerOffset, GameConfig::StalkerOffset)(rng);
        return std::clamp(plv + off, 1, GameConfig::MaxBotLevel);
    }
    }
    return 1;
}

inline BotTier roll_bot_tier_for_role(BotRole role, std::mt19937 &rng) {
    float r = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    switch (role) {
    case BotRole::Fodder:
        return BotTier::Normal;
    case BotRole::Stalker:
        if (r < GameConfig::BossRoll)
            return BotTier::Boss;
        else if (r < GameConfig::EliteRoll)
            return BotTier::Elite;
        else
            return BotTier::Normal;
    case BotRole::Brute:
        return (r < GameConfig::BruteEliteRoll) ? BotTier::Elite
                                                : BotTier::Boss;
    }
    return BotTier::Normal;
}

struct BotTierMult {
    float HpMul, AtkMul, AspMul, SpeedMul, VisionMul;
};

inline BotTierMult tier_mult(BotTier t) {
    switch (t) {
    case BotTier::Elite:
        return {
            GameConfig::EliteHpMul,
            GameConfig::EliteAtkMul,
            GameConfig::EliteAspMul,
            GameConfig::EliteSpeedMul,
            GameConfig::EliteVisionMul
        };
    case BotTier::Boss:
        return {
            GameConfig::BossHpMul,
            GameConfig::BossAtkMul,
            GameConfig::BossAspMul,
            GameConfig::BossSpeedMul,
            GameConfig::BossVisionMul
        };
    case BotTier::Normal:
    default:
        return {
            GameConfig::NormalHpMul,
            GameConfig::NormalAtkMul,
            GameConfig::NormalAspMul,
            GameConfig::NormalSpeedMul,
            GameConfig::NormalVisionMul
        };
    }
}

} // namespace detail
} // namespace sim
