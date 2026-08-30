#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../heroes/hero_registry.h"
#include "match_stats.h"
#include <cmath>
#include <entt/entt.hpp>

namespace sim {

inline void apply_xp(entt::registry &reg, entt::entity e, int xp_amount) {
    if (!reg.all_of<Experience, Level, MoveSpeed, Health>(e))
        return;
    auto &exp = reg.get<Experience>(e);
    auto &lv = reg.get<Level>(e);
    auto &ms = reg.get<MoveSpeed>(e);
    auto &hp = reg.get<Health>(e);
    const HeroDef *hero = nullptr;
    if (reg.all_of<HeroDefId>(e))
        hero = HeroRegistry::instance().find(reg.get<HeroDefId>(e).Value);
    float hp_per_level = hero ? hero->HpPerLevel : sim::stats(reg).HpPerLevel;
    float speed_per_level =
        hero ? hero->SpeedPerLevel : sim::stats(reg).SpeedPerLevel;
    float atk_per_level = hero ? hero->AtkPerLevel : sim::stats(reg).AtkPerLevel;
    float asp_per_level = hero ? hero->AspPerLevel : sim::stats(reg).AspPerLevel;

    exp.Cur += xp_amount;
    record_xp(reg, e, xp_amount);
    while (exp.Cur >= exp.Needed) {
        exp.Cur -= exp.Needed;
        lv.Value += 1;
        hp.Max += static_cast<int>(std::round(hp_per_level));
        apply_healing(reg, e, hp.Max);
        ms.Value += speed_per_level;
        if (reg.all_of<CombatStats>(e)) {
            auto &stats = reg.get<CombatStats>(e);
            stats.Atk += atk_per_level;
            stats.Asp = std::min(
                stats.Asp + asp_per_level, sim::stats(reg).AspMax
            );
        }
        exp.Needed = lv.Value * sim::stats(reg).XpPerLevelBase;
        if (reg.all_of<Mana>(e)) {
            auto &mana = reg.get<Mana>(e);
            mana.Cur = mana.Max;
        }
    }
}

} // namespace sim
