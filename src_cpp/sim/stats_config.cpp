#include "stats_config.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <set>
#include <type_traits>
#include <unordered_map>

namespace sim {
namespace {

using Values = std::unordered_map<std::string, std::string>;

template <typename T>
bool parse_number_value(const std::string &value, T &target);

std::string trim(std::string value) {
    auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string strip_comment(std::string value) {
    bool quoted = false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '"')
            quoted = !quoted;
        if (value[i] == '#' && !quoted)
            return value.substr(0, i);
    }
    return value;
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

bool parse_yaml(const std::string &text, Values &values, std::string &error) {
    struct Scope {
        int indent;
        std::string key;
    };
    std::vector<Scope> scopes;
    std::istringstream input(text);
    std::string line;
    int line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        if (line.find('\t') != std::string::npos) {
            error = "stats.yaml line " + std::to_string(line_number) +
                    " uses tabs for indentation";
            return false;
        }

        line = strip_comment(line);
        if (trim(line).empty() || trim(line) == "---")
            continue;

        int indent = 0;
        while (indent < static_cast<int>(line.size()) && line[indent] == ' ')
            ++indent;
        std::string content = trim(line.substr(indent));
        auto colon = content.find(':');
        if (colon == std::string::npos) {
            error = "stats.yaml line " + std::to_string(line_number) +
                    " must contain a key/value pair";
            return false;
        }

        std::string key = trim(content.substr(0, colon));
        std::string value = trim(content.substr(colon + 1));
        if (key.empty()) {
            error = "stats.yaml line " + std::to_string(line_number) +
                    " has an empty key";
            return false;
        }

        while (!scopes.empty() && indent <= scopes.back().indent)
            scopes.pop_back();

        std::string path;
        for (const auto &scope : scopes) {
            if (!path.empty())
                path += '.';
            path += scope.key;
        }
        if (!path.empty())
            path += '.';
        path += key;

        if (value.empty()) {
            scopes.push_back({indent, key});
            continue;
        }
        values[path] = unquote(value);
    }
    return true;
}

bool get_value(
    const Values &values,
    const std::string &key,
    std::string &value,
    std::string &error,
    bool required = false
) {
    auto it = values.find(key);
    if (it == values.end()) {
        if (required)
            error = "missing stats.yaml key: " + key;
        return false;
    }
    value = it->second;
    return true;
}

template <typename T>
bool parse_number(
    const Values &values,
    const std::string &key,
    T &target,
    std::string &error,
    bool required = false
) {
    std::string value;
    if (!get_value(values, key, value, error, required))
        return error.empty();

    char *end = nullptr;
    const char *begin = value.c_str();
    if constexpr (std::is_integral_v<T>) {
        long parsed = std::strtol(begin, &end, 10);
        if (end == begin || *end != '\0') {
            error = "invalid integer for stats.yaml key: " + key;
            return false;
        }
        target = static_cast<T>(parsed);
    } else {
        float parsed = std::strtof(begin, &end);
        if (end == begin || *end != '\0') {
            error = "invalid number for stats.yaml key: " + key;
            return false;
        }
        target = static_cast<T>(parsed);
    }
    return true;
}

bool parse_string(
    const Values &values,
    const std::string &key,
    std::string &target,
    std::string &error,
    bool required = false
) {
    std::string value;
    if (!get_value(values, key, value, error, required))
        return error.empty();
    target = value;
    return true;
}

bool parse_int_array(
    const Values &values,
    const std::string &key,
    int (&target)[4],
    std::string &error,
    bool required = false
) {
    std::string value;
    if (!get_value(values, key, value, error, required))
        return error.empty();
    value = trim(value);
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
        error = "invalid integer array for stats.yaml key: " + key;
        return false;
    }
    value = value.substr(1, value.size() - 2);
    std::stringstream stream(value);
    std::string item;
    int index = 0;
    while (std::getline(stream, item, ',')) {
        if (index >= 4 || !parse_number_value(trim(item), target[index])) {
            error = "invalid integer array for stats.yaml key: " + key;
            return false;
        }
        ++index;
    }
    if (index != 4) {
        error = "stats.yaml key must contain four integers: " + key;
        return false;
    }
    return true;
}

std::vector<std::string> section_names(
    const Values &values, const std::string &section
) {
    std::set<std::string> names;
    const std::string prefix = section + ".";
    for (const auto &[key, value] : values) {
        if (key.rfind(prefix, 0) != 0)
            continue;
        auto dot = key.find('.', prefix.size());
        if (dot == std::string::npos)
            continue;
        names.insert(key.substr(prefix.size(), dot - prefix.size()));
    }
    return {names.begin(), names.end()};
}

template <typename T>
bool parse_number_value(const std::string &value, T &target) {
    char *end = nullptr;
    if constexpr (std::is_integral_v<T>) {
        long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0')
            return false;
        target = static_cast<T>(parsed);
    } else {
        float parsed = std::strtof(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0')
            return false;
        target = static_cast<T>(parsed);
    }
    return true;
}

#define READ_FLOAT(path, field)                                                \
    if (!parse_number(values, path, config.field, error, true))                \
    return false
#define READ_INT(path, field)                                                  \
    if (!parse_number(values, path, config.field, error, true))                \
    return false

bool parse_skill(
    const Values &values,
    const std::string &name,
    SkillTuning &skill,
    std::string &error
) {
    const std::string prefix = "skills." + name + ".";
    std::string kind;
    if (!parse_number(values, prefix + "id", skill.Id, error, true) ||
        !parse_string(values, prefix + "name", skill.Name, error, true) ||
        !parse_string(values, prefix + "kind", kind, error, true) ||
        !parse_number(
            values, prefix + "base_cooldown", skill.BaseCooldown, error, true
        ) ||
        !parse_number(
            values, prefix + "base_mana_cost", skill.BaseManaCost, error, true
        ) ||
        !parse_number(
            values, prefix + "base_cast_time", skill.BaseCastTime, error, true
        ) ||
        !parse_number(
            values, prefix + "base_range", skill.BaseRange, error, true
        ) ||
        !parse_number(
            values,
            prefix + "cooldown_per_level",
            skill.CooldownPerLevel,
            error,
            true
        ) ||
        !parse_number(
            values,
            prefix + "mana_reduction_per_level",
            skill.ManaReductionPerLevel,
            error,
            true
        ) ||
        !parse_number(
            values,
            prefix + "mana_reduction_min",
            skill.ManaReductionMin,
            error,
            true
        ) ||
        !parse_number(
            values, prefix + "range_per_level", skill.RangePerLevel, error, true
        ) ||
        !parse_number(
            values, prefix + "damage_base", skill.DamageBase, error, true
        ) ||
        !parse_number(
            values,
            prefix + "damage_per_level",
            skill.DamagePerLevel,
            error,
            true
        ) ||
        !parse_number(
            values,
            prefix + "damage_atk_ratio",
            skill.DamageAtkRatio,
            error,
            true
        ) ||
        !parse_number(
            values, prefix + "effect_base", skill.EffectBase, error, true
        ) ||
        !parse_number(
            values,
            prefix + "effect_per_level",
            skill.EffectPerLevel,
            error,
            true
        ))
        return false;

    if (kind == "melee_single")
        skill.Kind = SkillKind::MeleeSingle;
    else if (kind == "aoe_field")
        skill.Kind = SkillKind::AoEField;
    else if (kind == "dash")
        skill.Kind = SkillKind::Dash;
    else if (kind == "channel_burst")
        skill.Kind = SkillKind::ChannelBurst;
    else if (kind == "terrain_rush")
        skill.Kind = SkillKind::TerrainRush;
    else if (kind == "target_teleport")
        skill.Kind = SkillKind::TargetTeleport;
    else if (kind == "radial_slow")
        skill.Kind = SkillKind::RadialSlow;
    else if (kind == "low_health_passive")
        skill.Kind = SkillKind::LowHealthPassive;
    else {
        error = "unknown skill kind in stats.yaml: " + kind;
        return false;
    }

    parse_string(values, prefix + "description", skill.Description, error);
    parse_number(values, prefix + "max_level", skill.MaxLevel, error);

    parse_number(values, prefix + "dash_speed", skill.DashSpeed, error);
    parse_number(
        values, prefix + "channel_interval", skill.ChannelInterval, error
    );
    parse_number(
        values,
        prefix + "channel_projectile_count",
        skill.ChannelProjectileCount,
        error
    );
    parse_number(
        values,
        prefix + "channel_projectile_spawn_radius",
        skill.ChannelProjectileSpawnRadius,
        error
    );
    parse_number(
        values, prefix + "modifier_magnitude", skill.ModifierMagnitude, error
    );
    parse_number(
        values, prefix + "modifier_duration", skill.ModifierDuration, error
    );
    parse_number(values, prefix + "crit_min", skill.CritMin, error);
    parse_number(values, prefix + "crit_max", skill.CritMax, error);
    parse_number(
        values, prefix + "lifesteal_min", skill.LifestealMin, error
    );
    parse_number(
        values, prefix + "lifesteal_max", skill.LifestealMax, error
    );
    parse_number(
        values, prefix + "crit_multiplier", skill.CritMultiplier, error
    );
    if (!error.empty())
        return false;
    return true;
}

} // namespace

bool load_stats_yaml(
    const std::string &text, StatsConfig &config, std::string &error
) {
    error.clear();
    Values values;
    if (!parse_yaml(text, values, error))
        return false;

    int version = 0;
    if (!parse_number(values, "schema.version", version, error, true))
        return false;
    if (version != 1) {
        error =
            "unsupported stats.yaml schema version: " + std::to_string(version);
        return false;
    }

    config = StatsConfig{};

    READ_FLOAT("simulation.tick_rate", TickRate);
    READ_FLOAT("simulation.snapshot_rate", SnapshotRate);
    READ_FLOAT("world.map_half", MapHalf);
    READ_FLOAT("player.radius", PlayerRadius);
    READ_FLOAT("player.speed", PlayerSpeed);
    READ_INT("player.base_hp", PlayerBaseHp);
    READ_FLOAT("player.base_attack", BaseAttack);
    READ_FLOAT("player.base_attack_speed", BaseAttackSpeed);
    READ_FLOAT("progression.atk_per_kill", AtkPerKill);
    READ_FLOAT("progression.asp_per_kill", AspPerKill);
    READ_FLOAT("progression.asp_max", AspMax);
    READ_FLOAT("projectile.speed", ArrowSpeed);
    READ_FLOAT("projectile.lifetime", ArrowLifetime);
    READ_FLOAT("projectile.radius", ArrowRadius);

    READ_INT("bot.count", BotCount);
    READ_FLOAT("bot.radius", BotRadius);
    READ_FLOAT("bot.speed", BotSpeed);
    READ_INT("bot.hp", BotHp);
    READ_FLOAT("bot.base_attack", BotBaseAttack);
    READ_FLOAT("bot.base_attack_speed", BotBaseAttackSpeed);
    READ_FLOAT("bot.respawn_time", BotRespawnTime);
    READ_FLOAT("bot.vision_range", BotVisionRange);
    READ_FLOAT("bot.stat_multiplier", BotStatMul);
    READ_FLOAT("bot.wander_interval_min", BotWanderIntervalMin);
    READ_FLOAT("bot.wander_interval_max", BotWanderIntervalMax);

    READ_INT("hero_progression.max_level", MaxHeroLevel);
    READ_FLOAT("hero_progression.atk_per_level", AtkPerLevel);
    READ_FLOAT("hero_progression.asp_per_level", AspPerLevel);

    READ_FLOAT("bot_tiers.normal.hp_multiplier", NormalHpMul);
    READ_FLOAT("bot_tiers.normal.attack_multiplier", NormalAtkMul);
    READ_FLOAT("bot_tiers.normal.attack_speed_multiplier", NormalAspMul);
    READ_FLOAT("bot_tiers.normal.speed_multiplier", NormalSpeedMul);
    READ_FLOAT("bot_tiers.normal.vision_multiplier", NormalVisionMul);
    READ_FLOAT("bot_tiers.elite.hp_multiplier", EliteHpMul);
    READ_FLOAT("bot_tiers.elite.attack_multiplier", EliteAtkMul);
    READ_FLOAT("bot_tiers.elite.attack_speed_multiplier", EliteAspMul);
    READ_FLOAT("bot_tiers.elite.speed_multiplier", EliteSpeedMul);
    READ_FLOAT("bot_tiers.elite.vision_multiplier", EliteVisionMul);
    READ_FLOAT("bot_tiers.boss.hp_multiplier", BossHpMul);
    READ_FLOAT("bot_tiers.boss.attack_multiplier", BossAtkMul);
    READ_FLOAT("bot_tiers.boss.attack_speed_multiplier", BossAspMul);
    READ_FLOAT("bot_tiers.boss.speed_multiplier", BossSpeedMul);
    READ_FLOAT("bot_tiers.boss.vision_multiplier", BossVisionMul);
    READ_FLOAT("bot_tiers.boss_roll", BossRoll);
    READ_FLOAT("bot_tiers.elite_roll", EliteRoll);

    READ_INT("bot_roles.fodder_max_level", FodderMaxLv);
    READ_INT("bot_roles.brute_min_level", BruteMinLv);
    READ_INT("bot_roles.stalker_level_offset", StalkerOffset);
    READ_INT("bot_roles.fodder_weight", FodderWeight);
    READ_INT("bot_roles.stalker_weight", StalkerWeight);
    READ_INT("bot_roles.brute_weight", BruteWeight);
    READ_FLOAT("bot_roles.brute_elite_roll", BruteEliteRoll);

    READ_FLOAT("bot_ai.decision_cooldown", BotDecisionCooldown);
    READ_FLOAT("bot_ai.flee_distance", BotFleeDist);
    READ_FLOAT("bot_ai.engage_range_high", BotEngageRangeHigh);
    READ_FLOAT("bot_ai.engage_range_low", BotEngageRangeLow);
    READ_FLOAT("bot_ai.kite_strafe_distance", BotKiteStrafeDist);
    READ_FLOAT("bot_ai.target_lock_time", BotTargetLockTime);
    READ_FLOAT("bot_ai.goal_commit_time", BotGoalCommitTime);
    READ_FLOAT("bot_ai.kite_chase_exit", BotKiteChaseExit);
    READ_FLOAT("bot_ai.kite_chase_enter", BotKiteChaseEnter);
    READ_FLOAT("bot_ai.kite_retreat_exit", BotKiteRetreatExit);
    READ_FLOAT("bot_ai.kite_retreat_enter", BotKiteRetreatEnter);

    READ_FLOAT("bot_combat.phase_cooldown", BotCombatPhaseCooldown);
    READ_FLOAT("bot_combat.skill_decision_cooldown", BotSkillDecisionCooldown);
    READ_FLOAT("bot_combat.goal_decision_cooldown", BotGoalDecisionCooldown);
    READ_FLOAT("bot_combat.approach_threshold", BotApproachThreshold);
    READ_FLOAT("bot_combat.kite_threshold", BotKiteThreshold);
    READ_FLOAT("bot_combat.burst_health_threshold", BotBurstHealthThreshold);
    READ_FLOAT(
        "bot_combat.sustain_health_threshold", BotSustainHealthThreshold
    );
    READ_FLOAT(
        "bot_combat.disengage_health_threshold", BotDisengageHealthThreshold
    );
    READ_FLOAT(
        "bot_combat.safe_distance_multiplier", BotSafeDistanceMultiplier
    );
    READ_FLOAT("bot_combat.burst_step_limit", BotBurstStepLimit);
    READ_FLOAT("bot_combat.burst_duration", BotBurstDuration);
    READ_FLOAT(
        "bot_combat.disengage_recovery_threshold", BotDisengageRecoveryThreshold
    );

    READ_FLOAT("progression.kill_xp_base", KillXpBase);
    READ_FLOAT("progression.kill_xp_high_bonus", KillXpHighBonus);
    READ_INT("progression.xp_per_level_base", XpPerLevelBase);
    READ_INT("progression.hp_per_level", HpPerLevel);
    READ_FLOAT("progression.speed_per_level", SpeedPerLevel);
    READ_FLOAT("progression.heal_fraction", HealFraction);

    READ_INT("entity_ids.player_start", PlayerIdStart);
    READ_INT("entity_ids.bot_start", BotIdStart);
    READ_INT("entity_ids.arrow_start", ArrowIdStart);
    READ_INT("entity_ids.pickup_start", PickupIdStart);
    READ_INT("entity_ids.aoe_start", AoEIdStart);
    READ_FLOAT("player.spawn_safe_radius", PlayerSpawnSafeRadius);

    READ_INT("pickups.xp.value", XpPickupValue);
    READ_INT("pickups.heal.value", HealPickupValue);
    READ_INT("pickups.small_heal.value", SmallHealPickupValue);
    READ_FLOAT("pickups.xp.respawn_time", XpPickupRespawnTime);
    READ_FLOAT("pickups.heal.respawn_time", HealPickupRespawnTime);
    READ_FLOAT("pickups.small_heal.respawn_time", SmallHealPickupRespawnTime);
    READ_FLOAT("pickups.radius", PickupRadius);
    READ_INT("pickups.xp.count", XpPickupCount);
    READ_INT("pickups.heal.count", HealPickupCount);
    READ_INT("pickups.small_heal.count", SmallHealPickupCount);

    READ_FLOAT("pathfinding.repath_target_deadzone", RepathTargetDeadzone);
    READ_FLOAT("pathfinding.turn_rate", PathTurnRate);
    READ_FLOAT(
        "pathfinding.skill_chase_repath_deadzone", SkillChaseRepathDeadzone
    );
    READ_FLOAT("mana.player_max", PlayerBaseMana);
    READ_FLOAT("mana.player_regen", PlayerManaRegen);
    READ_FLOAT("mana.bot_max", BotBaseMana);
    READ_FLOAT("mana.bot_regen", BotManaRegen);
    READ_FLOAT("mana.regen_delay", ManaRegenDelay);
    READ_FLOAT("attack.acquisition_range", AttackAcquisitionRange);

    READ_INT("skills.max_level", MaxSkillLevel);
    READ_FLOAT("skills.mana_reduction_min", SkillManaReductionMin);
    READ_FLOAT("skills.cdr_min", SkillCDRMin);
    READ_FLOAT("skills.cdr_per_level", SkillCDRPerLevel);
    READ_FLOAT("skills.damage_attack_ratio", SkillDamageAtkRatio);
    READ_FLOAT("skills.bot_damage_multiplier", BotSkillDmgMul);
    READ_FLOAT("skills.bot_cooldown_multiplier", BotSkillCooldownMul);
    READ_FLOAT("skills.bot_mana_multiplier", BotManaCostMul);

    READ_FLOAT("bot_skill_scoring.base", BotSkillScoreBase);
    READ_FLOAT("bot_skill_scoring.in_range", BotSkillScoreInRange);
    READ_FLOAT("bot_skill_scoring.near_range", BotSkillScoreNearRange);
    READ_FLOAT("bot_skill_scoring.out_of_range", BotSkillScoreOutOfRange);
    READ_FLOAT("bot_skill_scoring.approach_dash", BotSkillScoreApproachDash);
    READ_FLOAT("bot_skill_scoring.approach_melee", BotSkillScoreApproachMelee);
    READ_FLOAT("bot_skill_scoring.kite_aoe", BotSkillScoreKiteAoe);
    READ_FLOAT("bot_skill_scoring.kite_channel", BotSkillScoreKiteChannel);
    READ_FLOAT("bot_skill_scoring.burst_melee", BotSkillScoreBurstMelee);
    READ_FLOAT("bot_skill_scoring.burst_aoe", BotSkillScoreBurstAoe);
    READ_FLOAT(
        "bot_skill_scoring.sustain_channel", BotSkillScoreSustainChannel
    );
    READ_FLOAT("bot_skill_scoring.sustain_aoe", BotSkillScoreSustainAoe);
    READ_FLOAT("bot_skill_scoring.disengage_dash", BotSkillScoreDisengageDash);
    READ_FLOAT("bot_skill_scoring.low_health_dash", BotSkillScoreLowHealthDash);
    READ_FLOAT(
        "bot_skill_scoring.high_health_channel", BotSkillScoreHighHealthChannel
    );
    READ_FLOAT(
        "bot_skill_scoring.low_health_channel", BotSkillScoreLowHealthChannel
    );
    READ_FLOAT("bot_skill_scoring.aoe_enemy", BotSkillScoreAoeEnemy);
    READ_FLOAT(
        "bot_skill_scoring.enemy_scan_radius_sq", BotSkillEnemyScanRadiusSq
    );
    READ_FLOAT("bot_skill_scoring.low_health", BotSkillLowHealth);
    READ_FLOAT("bot_skill_scoring.high_health", BotSkillHighHealth);
    READ_FLOAT("bot_skill_scoring.sustain_health", BotSkillSustainHealth);

    auto hero_names = section_names(values, "heroes");
    if (hero_names.empty()) {
        error = "stats.yaml must define at least one hero";
        return false;
    }
    std::set<int> hero_ids;
    std::set<int> prefab_ids;
    for (const auto &name : hero_names) {
        const std::string prefix = "heroes." + name + ".";
        HeroDef hero;
        std::string attack_type;
        if (!parse_number(values, prefix + "id", hero.Id, error, true) ||
            !parse_string(values, prefix + "name", hero.Name, error, true) ||
            !parse_string(values, prefix + "role", hero.Role, error, true) ||
            !parse_string(
                values, prefix + "description", hero.Description, error, true
            ) ||
            !parse_int_array(
                values, prefix + "skill_ids", hero.SkillIds, error, true
            ) ||
            !parse_number(
                values, prefix + "base_hp", hero.BaseHp, error, true
            ) ||
            !parse_number(
                values, prefix + "base_mana", hero.BaseMana, error, true
            ) ||
            !parse_number(
                values, prefix + "base_attack", hero.BaseAtk, error, true
            ) ||
            !parse_number(
                values,
                prefix + "base_attack_speed",
                hero.BaseAsp,
                error,
                true
            ) ||
            !parse_number(
                values,
                prefix + "base_move_speed",
                hero.BaseMoveSpeed,
                error,
                true
            ) ||
            !parse_number(
                values, prefix + "attack_range", hero.AttackRange, error, true
            ) ||
            !parse_number(
                values, prefix + "hp_per_level", hero.HpPerLevel, error, true
            ) ||
            !parse_number(
                values,
                prefix + "attack_per_level",
                hero.AtkPerLevel,
                error,
                true
            ) ||
            !parse_number(
                values,
                prefix + "attack_speed_per_level",
                hero.AspPerLevel,
                error,
                true
            ) ||
            !parse_number(
                values,
                prefix + "move_speed_per_level",
                hero.SpeedPerLevel,
                error,
                true
            ) ||
            !parse_number(
                values, prefix + "prefab_id", hero.PrefabId, error, true
            ) ||
            !parse_string(
                values, prefix + "attack_type", attack_type, error, true
            ))
            return false;
        if (hero.Id <= 0 || !hero_ids.insert(hero.Id).second ||
            hero.PrefabId < 0 || !prefab_ids.insert(hero.PrefabId).second) {
            error = "duplicate or invalid hero id/prefab_id in stats.yaml";
            return false;
        }
        if (attack_type == "projectile")
            hero.AttackType = AttackDelivery::Projectile;
        else if (attack_type == "melee")
            hero.AttackType = AttackDelivery::Melee;
        else {
            error = "unknown hero attack_type in stats.yaml: " + attack_type;
            return false;
        }
        config.Heroes.push_back(std::move(hero));
    }

    auto skill_names = section_names(values, "skills");
    if (skill_names.empty()) {
        error = "stats.yaml must define at least one skill";
        return false;
    }
    for (const auto &name : skill_names) {
        SkillTuning skill;
        if (!parse_skill(values, name, skill, error))
            return false;
        if (skill.Id <= 0 || skill.MaxLevel < 1) {
            error = "invalid skill id or max_level in stats.yaml";
            return false;
        }
        if (!config.Skills.emplace(skill.Id, std::move(skill)).second) {
            error = "duplicate skill id in stats.yaml";
            return false;
        }
    }

    for (const auto &hero : config.Heroes) {
        for (int skill_id : hero.SkillIds) {
            if (config.Skills.find(skill_id) == config.Skills.end()) {
                error = "hero references unknown skill id: " +
                        std::to_string(skill_id);
                return false;
            }
        }
    }

    if (config.BotCount < 0 || config.MaxHeroLevel < 1 ||
        config.MaxSkillLevel < 1 || config.FodderWeight < 0 ||
        config.StalkerWeight < 0 || config.BruteWeight < 0) {
        error = "stats.yaml contains a negative count or invalid level limit";
        return false;
    }
    config.derive();
    return true;
}

#undef READ_FLOAT
#undef READ_INT

} // namespace sim
