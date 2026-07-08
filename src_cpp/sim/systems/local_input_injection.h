#pragma once

#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void local_input_injection_system(
    entt::registry &reg, entt::entity local_input_entity
) {
    auto &input = reg.get<LocalInputSingleton>(local_input_entity);
    auto view = reg.view<PlayerInputState, PlayerTag>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &state = view.get<PlayerInputState>(e);
        state.Move = input.Move;
        state.Aim = input.Aim;
        state.Fire = input.Fire;
        state.Seq = input.Seq;
        state.CastSlot = input.CastSlot;
        state.CastConfirm = input.CastConfirm;
        state.CastCancel = input.CastCancel;
        state.CastInterrupt = input.CastInterrupt;
        state.CastAim = input.CastAim;
    }
}

} // namespace sim
