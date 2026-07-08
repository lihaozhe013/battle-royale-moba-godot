#pragma once

#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void skill_cooldown_system(entt::registry &reg, float dt) {
    auto view = reg.view<SkillComponent>();
    for (auto e : view) {
        auto &skills = view.get<SkillComponent>(e);
        for (int i = 0; i < 4; ++i) {
            auto &slot = skills.Slots[i];
            if (slot.CooldownTimer > 0.0f) {
                slot.CooldownTimer = std::max(0.0f, slot.CooldownTimer - dt);
            }
        }
    }
}

} // namespace sim
