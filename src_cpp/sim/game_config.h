#pragma once

#include "stats_config.h"
#include <entt/entt.hpp>

namespace sim {

inline StatsConfig &stats(entt::registry &reg) {
    return reg.ctx().get<StatsConfig>();
}

} // namespace sim
