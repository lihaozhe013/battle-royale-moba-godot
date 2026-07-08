#pragma once

#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void arrow_movement_system(entt::registry &reg, float dt) {
    auto view = reg.view<ArrowTag, Position2D, Velocity2D, Lifetime>();
    for (auto e : view) {
        auto &pos = view.get<Position2D>(e);
        auto &vel = view.get<Velocity2D>(e);
        auto &life = view.get<Lifetime>(e);

        pos.Value = pos.Value + vel.Value * dt;
        life.Remaining -= dt;
    }
}

} // namespace sim
