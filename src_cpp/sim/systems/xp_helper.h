#pragma once

#include "../components.h"
#include "../game_config.h"
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
    while (exp.Cur >= exp.Needed) {
        exp.Cur -= exp.Needed;
        lv.Value += 1;
        hp.Max += GameConfig::HpPerLevel;
        hp.Cur = hp.Max;
        ms.Value += GameConfig::SpeedPerLevel;
        exp.Needed = lv.Value * GameConfig::XpPerLevelBase;
    }
}

} // namespace sim
