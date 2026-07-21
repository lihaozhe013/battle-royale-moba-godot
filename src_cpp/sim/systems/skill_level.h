#pragma once

#include "../components.h"
#include "../game_config.h"
#include <entt/entt.hpp>

namespace sim {

inline void skill_level_system(entt::registry &reg) {
    auto view =
        reg.view<PlayerTag, PlayerInputState, SkillComponent, SkillPoints>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &sp = view.get<SkillPoints>(e);

        if (input.SkillUpgradeSlot < 0 || input.SkillUpgradeSlot >= 4)
            continue;
        if (sp.Available <= 0)
            continue;

        auto &slot = skills.Slots[input.SkillUpgradeSlot];
        if (slot.SkillId <= 0 || slot.Level >= GameConfig::MaxSkillLevel)
            continue;

        slot.Level++;
        sp.Available--;
    }
}

} // namespace sim
