#pragma once

#include "../snapshot_builder.h"
#include <entt/entt.hpp>

namespace sim {

inline bool snapshot_export_system(
    entt::registry &reg,
    int &tick_counter,
    double match_time,
    int result,
    godot::Ref<SimSnapshot> &out_snap
) {
    tick_counter++;
    if (tick_counter % 1 != 0) {
        return false;
    }
    out_snap = SnapshotBuilder::build(reg, tick_counter, match_time, result);
    return true;
}

} // namespace sim
