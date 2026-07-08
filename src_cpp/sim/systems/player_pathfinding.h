#pragma once

#include "../components.h"
#include "../nav_grid.h"
#include <entt/entt.hpp>

namespace sim {

inline void player_pathfinding_system(
    entt::registry &reg, const NavGrid &nav
) {
    auto view = reg.view<
        PlayerTag, Position2D, PlayerInputState, CastState, MovePath>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &cs = view.get<CastState>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);

        // Any cast active → discard move command, stop following
        if (cs.State != CastState::Phase::None) {
            path.Following = false;
            continue;
        }

        if (input.MoveIssue) {
            auto waypoints = nav.find_path(pos.Value, input.MoveTarget);
            if (!waypoints.empty()) {
                path.Waypoints = std::move(waypoints);
                path.CurrentIndex = 0;
                path.FinalTarget = input.MoveTarget;
                path.Following = true;
            }
        }
    }
}

} // namespace sim
