#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"

namespace sim {

inline void skill_input_system(entt::registry &reg) {
    auto view = reg.view<PlayerTag, PlayerInputState, SkillComponent, Mana>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);

        bool press[4] = {input.SkillQ, input.SkillW, input.SkillE, input.SkillR};
        for (int i = 0; i < 4; ++i) {
            if (!press[i]) continue;
            auto &slot = skills.Slots[i];
            if (slot.SkillId <= 0) continue;
            if (slot.CooldownTimer > 0.0f) continue;
            if (mana.Cur < slot.ManaCost) continue;
            mana.Cur -= slot.ManaCost;
            mana.RegenTimer = GameConfig::ManaRegenDelay;
            slot.CooldownTimer = slot.MaxCooldown;
        }
    }
}

} // namespace sim
