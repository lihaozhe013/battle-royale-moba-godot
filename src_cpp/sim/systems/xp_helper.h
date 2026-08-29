#pragma once

#include "../components.h"
#include "../game_config.h"
#include "match_stats.h"
#include <entt/entt.hpp>

namespace sim {

inline void apply_xp(entt::registry &reg, entt::entity e, int xp_amount) {
    if (!reg.all_of<Experience, Level, MoveSpeed, Health>(e))
        return;
    auto &exp = reg.get<Experience>(e);
    auto &lv = reg.get<Level>(e);
    auto &ms = reg.get<MoveSpeed>(e);
    auto &hp = reg.get<Health>(e);

    exp.Cur += xp_amount;
    record_xp(reg, e, xp_amount);
    while (exp.Cur >= exp.Needed) {
        exp.Cur -= exp.Needed;
        lv.Value += 1;
        hp.Max += sim::stats(reg).HpPerLevel;
        apply_healing(reg, e, hp.Max);
        ms.Value += sim::stats(reg).SpeedPerLevel;
        if (reg.all_of<CombatStats>(e)) {
            auto &stats = reg.get<CombatStats>(e);
            stats.Atk += sim::stats(reg).AtkPerLevel;
            stats.Asp = std::min(
                stats.Asp + sim::stats(reg).AspPerLevel, sim::stats(reg).AspMax
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
