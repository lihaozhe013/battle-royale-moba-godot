#pragma once

#include <entt/entt.hpp>
#include "../snapshot_builder.h"

namespace sim {

inline bool snapshot_export_system(entt::registry &reg, int &tick_counter,
                                    godot::Ref<SimSnapshot> &out_snap) {
    tick_counter++;
    if (tick_counter % 1 != 0) {
        return false;
    }
    out_snap = SnapshotBuilder::build(reg, tick_counter);
    return true;
}

} // namespace sim
