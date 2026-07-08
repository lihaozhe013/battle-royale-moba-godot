#pragma once

#include "../command_buffer.h"
#include "../components.h"
#include <entt/entt.hpp>
#include <vector>

namespace sim {

inline void aoe_system(entt::registry &reg, float dt, CommandBuffer &cb) {
    auto view = reg.view<AoETag>();
    std::vector<entt::entity> to_destroy;
    for (auto e : view) {
        auto &a = view.get<AoETag>(e);
        a.Timer -= dt;
        if (a.Timer <= 0.0f) {
            to_destroy.push_back(e);
        }
    }
    for (auto e : to_destroy) {
        cb.push([e](entt::registry &r) { r.destroy(e); });
    }
}

} // namespace sim
