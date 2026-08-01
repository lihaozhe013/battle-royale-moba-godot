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
        return std::uniform_int_distribution<int>(1, stats(reg).FodderMaxLv)(
            rng
        );
    case BotRole::Brute:
        return std::uniform_int_distribution<
            int>(stats(reg).BruteMinLv, stats(reg).MaxHeroLevel)(rng);
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
            int>(-stats(reg).StalkerOffset, stats(reg).StalkerOffset)(rng);
        return std::clamp(plv + off, 1, stats(reg).MaxHeroLevel);
    }
    }
    return 1;
}

inline BotTier roll_bot_tier_for_role(
    BotRole role, std::mt19937 &rng, const StatsConfig &config
) {
    float r = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    switch (role) {
    case BotRole::Fodder:
        return BotTier::Normal;
    case BotRole::Stalker:
        if (r < config.BossRoll)
            return BotTier::Boss;
        else if (r < config.EliteRoll)
            return BotTier::Elite;
        else
            return BotTier::Normal;
    case BotRole::Brute:
        return (r < config.BruteEliteRoll) ? BotTier::Elite : BotTier::Boss;
    }
    return BotTier::Normal;
}

struct BotTierMult {
    float HpMul, AtkMul, AspMul, SpeedMul, VisionMul;
};

inline BotTierMult tier_mult(BotTier t, const StatsConfig &config) {
    switch (t) {
    case BotTier::Elite:
        return {
            config.EliteHpMul,
            config.EliteAtkMul,
            config.EliteAspMul,
            config.EliteSpeedMul,
            config.EliteVisionMul
        };
    case BotTier::Boss:
        return {
            config.BossHpMul,
            config.BossAtkMul,
            config.BossAspMul,
            config.BossSpeedMul,
            config.BossVisionMul
        };
    case BotTier::Normal:
    default:
        return {
            config.NormalHpMul,
            config.NormalAtkMul,
            config.NormalAspMul,
            config.NormalSpeedMul,
            config.NormalVisionMul
        };
    }
}

} // namespace detail
} // namespace sim
