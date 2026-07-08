#pragma once

#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void status_effect_system(entt::registry &reg, float dt) {
    auto view = reg.view<StatusEffect>();
    for (auto e : view) {
        auto &s = view.get<StatusEffect>(e);
        if (s.Type == StatusType::None)
            continue;
        s.Timer -= dt;
        if (s.Timer <= 0.0f) {
            s.Type = StatusType::None;
            s.Timer = 0.0f;
        }
    }
}

} // namespace sim
