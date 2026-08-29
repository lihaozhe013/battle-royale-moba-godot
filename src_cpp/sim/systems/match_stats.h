#pragma once

#include "../components.h"
#include "../game_config.h"
#include <algorithm>
#include <entt/entt.hpp>

namespace sim {

inline void _push_kill_event(
    entt::registry &reg, int killer_id, int victim_id
) {
    auto view = reg.view<KillEventBuffer>();
    for (auto e : view)
        view.get<KillEventBuffer>(e).events.push_back({killer_id, victim_id});
}

inline int apply_damage(
    entt::registry &reg,
    entt::entity attacker,
    entt::entity target,
    int requested_damage
) {
    if (!reg.valid(target) || !reg.all_of<Health>(target) ||
        requested_damage <= 0)
        return 0;

    auto &health = reg.get<Health>(target);
    int current_hp = std::max(0, health.Cur);
    int actual_damage = std::min(current_hp, requested_damage);
    if (actual_damage <= 0)
        return 0;

    health.Cur = current_hp - actual_damage;

    if (reg.valid(attacker) && reg.all_of<MatchStats>(attacker))
        reg.get<MatchStats>(attacker).DamageDealt += actual_damage;
    if (reg.all_of<MatchStats>(target))
        reg.get<MatchStats>(target).DamageTaken += actual_damage;

    if (health.Cur > 0)
        return actual_damage;

    health.Cur = 0;
    bool newly_dead = !reg.all_of<Dead>(target) ||
                      !reg.get<Dead>(target).enabled;
    if (reg.all_of<Dead>(target))
        reg.get<Dead>(target).enabled = true;
    if (reg.all_of<BotAIState>(target))
        reg.get<BotAIState>(target).RespawnTimer = stats(reg).BotRespawnTime;

    if (reg.all_of<MatchStats>(target))
        reg.get<MatchStats>(target).Deaths += 1;

    if (!newly_dead || !reg.valid(attacker) || attacker == target ||
        !reg.all_of<NetworkId>(attacker) || !reg.all_of<NetworkId>(target))
        return actual_damage;

    int killer_id = reg.get<NetworkId>(attacker).Value;
    int victim_id = reg.get<NetworkId>(target).Value;
    _push_kill_event(reg, killer_id, victim_id);

    if (reg.all_of<Kills>(attacker))
        reg.get<Kills>(attacker).Value += 1;

    return actual_damage;
}

inline int apply_healing(
    entt::registry &reg, entt::entity healer, int requested_healing
) {
    if (!reg.valid(healer) || !reg.all_of<Health>(healer) ||
        requested_healing <= 0)
        return 0;

    auto &health = reg.get<Health>(healer);
    int current_hp = std::clamp(health.Cur, 0, health.Max);
    int actual_healing = std::min(requested_healing, health.Max - current_hp);
    if (actual_healing <= 0)
        return 0;

    health.Cur = current_hp + actual_healing;
    if (reg.all_of<MatchStats>(healer))
        reg.get<MatchStats>(healer).HealingDone += actual_healing;
    return actual_healing;
}

inline void record_xp(entt::registry &reg, entt::entity e, int amount) {
    if (amount > 0 && reg.valid(e) && reg.all_of<MatchStats>(e))
        reg.get<MatchStats>(e).XpEarned += amount;
}

inline void record_skill_cast(entt::registry &reg, entt::entity e) {
    if (reg.valid(e) && reg.all_of<MatchStats>(e))
        reg.get<MatchStats>(e).SkillCasts += 1;
}

inline int calculate_match_score(
    int kills, int level, const MatchStats &match_stats
) {
    return kills * 100 + match_stats.DamageDealt / 10 +
           match_stats.HealingDone / 20 + match_stats.XpEarned / 5 +
           level * 50 - match_stats.Deaths * 75;
}

} // namespace sim
