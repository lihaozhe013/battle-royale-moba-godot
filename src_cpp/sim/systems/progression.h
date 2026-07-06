#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"
#include "xp_helper.h"

namespace sim {

inline void progression_system(entt::registry &reg) {
    auto kill_view = reg.view<KillEventBuffer>();
    if (kill_view.begin() == kill_view.end()) return;

    for (auto k : kill_view) {
        auto &buf = kill_view.get<KillEventBuffer>(k);
        if (buf.events.empty()) continue;

        auto combatant_view = reg.view<NetworkId, CombatStats>();

        for (auto &ev : buf.events) {
            // ── Atk/Asp per kill (existing) ──
            for (auto c : combatant_view) {
                auto &net = combatant_view.get<NetworkId>(c);
                if (net.Value != ev.KillerId) continue;
                auto &stats = combatant_view.get<CombatStats>(c);
                stats.Atk += GameConfig::AtkPerKill;
                stats.Asp = std::min(stats.Asp + GameConfig::AspPerKill, GameConfig::AspMax);
                break;
            }

            // ── Kill XP (new) ──
            int victim_lv = 1;
            int killer_lv = 1;
            entt::entity killer_entity = entt::null;
            entt::entity victim_entity = entt::null;

            auto lv_view = reg.view<NetworkId, Level>();
            for (auto x : lv_view) {
                auto &n = lv_view.get<NetworkId>(x);
                if (n.Value == ev.VictimId) {
                    victim_lv = lv_view.get<Level>(x).Value;
                    victim_entity = x;
                }
                if (n.Value == ev.KillerId) {
                    killer_lv = lv_view.get<Level>(x).Value;
                    killer_entity = x;
                }
            }

            if (killer_entity != entt::null && killer_entity != victim_entity) {
                int level_diff = std::max(0, victim_lv - killer_lv);
                int kill_xp = static_cast<int>(
                    GameConfig::KillXpBase * victim_lv * (1.0f + level_diff * GameConfig::KillXpHighBonus)
                );
                apply_xp(reg, killer_entity, kill_xp);
            }
        }
        buf.events.clear();
    }
}

} // namespace sim
