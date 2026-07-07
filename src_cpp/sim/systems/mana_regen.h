#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"

namespace sim {

inline void mana_regen_system(entt::registry &reg, float dt) {
    auto view = reg.view<Mana>();
    for (auto e : view) {
        auto &mana = view.get<Mana>(e);
        mana.RegenTimer -= dt;
        if (mana.RegenTimer <= 0.0f) {
            mana.Cur = std::min(mana.Cur + mana.RegenRate * dt, mana.Max);
        }
    }
}

} // namespace sim
