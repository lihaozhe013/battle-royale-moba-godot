#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"

namespace sim {

inline void progression_system(entt::registry &reg) {
    auto kill_view = reg.view<KillEventBuffer>();
    if (kill_view.begin() == kill_view.end()) return;

    for (auto k : kill_view) {
        auto &buf = kill_view.get<KillEventBuffer>(k);
        if (buf.events.empty()) continue;

        auto combatant_view = reg.view<NetworkId, CombatStats>();
        for (auto &ev : buf.events) {
            for (auto c : combatant_view) {
                auto &net = combatant_view.get<NetworkId>(c);
                if (net.Value != ev.KillerId) continue;
                auto &stats = combatant_view.get<CombatStats>(c);
                stats.Atk += GameConfig::AtkPerKill;
                stats.Asp = std::min(stats.Asp + GameConfig::AspPerKill, GameConfig::AspMax);
                break;
            }
        }
        buf.events.clear();
    }
}

} // namespace sim
