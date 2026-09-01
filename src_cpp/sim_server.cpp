#include "sim_server.h"
#include "sim/heroes/hero_registry.h"
#include "sim/skills/skill_registry.h"
#include "sim/stats_config.h"
#include <algorithm>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

SimServer::SimServer() {}
SimServer::~SimServer() {}

void SimServer::_bind_methods() {
    // ── 核心方法 ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("initialize", "map_json", "stats_yaml", "local_hero_id"),
        &SimServer::initialize,
        DEFVAL(1));
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_hero_catalog", "stats_yaml"),
        &SimServer::get_hero_catalog);

    // ── v2 新命令 API ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_skill_command", "slot", "confirm", "aim_x", "aim_y", "target_id"),
        &SimServer::set_skill_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_skill_upgrade_command", "slot"),
        &SimServer::set_skill_upgrade_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_attack_command_full", "target_id", "ground", "gx", "gy", "clear"),
        &SimServer::set_attack_command_full);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_cancel_command", "skill", "attack"),
        &SimServer::set_cancel_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_move_command", "target_x", "target_y", "issue"),
        &SimServer::set_move_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_stop_command", "stop"),
        &SimServer::set_stop_command);

    // ── v1 旧 API（deprecated，临时保留兼容） ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_local_input", "move", "aim", "fire", "seq"),
        &SimServer::set_local_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_cast_input", "cast_slot", "confirm", "cancel", "interrupt", "aim_x", "aim_y", "target_id"),
        &SimServer::set_cast_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_attack_command", "target_id"),
        &SimServer::set_attack_command);

    godot::ClassDB::bind_method(
        godot::D_METHOD("tick", "delta"),
        &SimServer::tick);
    godot::ClassDB::bind_method(
        godot::D_METHOD("pop_snapshot"),
        &SimServer::pop_snapshot);
    godot::ClassDB::bind_method(
        godot::D_METHOD("is_game_over"),
        &SimServer::is_game_over);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_hero_capacity"),
        &SimServer::get_hero_capacity);
    godot::ClassDB::bind_method(
        godot::D_METHOD("get_perf_stats"),
        &SimServer::get_perf_stats);
}

bool SimServer::initialize(
    const godot::String &p_map_json,
    const godot::String &p_stats_yaml,
    int local_hero_id
) {
    bool initialized = _world.initialize(
        p_map_json.utf8().get_data(),
        p_stats_yaml.utf8().get_data(),
        local_hero_id
    );
    if (!initialized) {
        godot::UtilityFunctions::push_error(
            godot::String("[stats_config] Failed to load data/stats.yaml: ") +
            godot::String(_world.last_error().c_str())
        );
    }
    return initialized;
}

godot::Array SimServer::get_hero_catalog(
    const godot::String &p_stats_yaml
) const {
    sim::StatsConfig config;
    std::string error;
    godot::Array catalog;
    if (!sim::load_stats_yaml(p_stats_yaml.utf8().get_data(), config, error)) {
        godot::UtilityFunctions::push_error(
            godot::String("[stats_config] Failed to load hero catalog: ") +
            godot::String(error.c_str())
        );
        return catalog;
    }

    std::vector<const sim::HeroDef *> heroes;
    heroes.reserve(config.Heroes.size());
    for (const auto &hero : config.Heroes)
        heroes.push_back(&hero);
    std::sort(
        heroes.begin(),
        heroes.end(),
        [](const sim::HeroDef *a, const sim::HeroDef *b) {
            return a->Id < b->Id;
        }
    );

    auto target_mode = [](sim::SkillKind kind) {
        switch (kind) {
        case sim::SkillKind::MeleeSingle:
        case sim::SkillKind::TargetTeleport:
            return sim::SkillTargetMode::Unit;
        case sim::SkillKind::AoEField:
        case sim::SkillKind::Dash:
            return sim::SkillTargetMode::Point;
        case sim::SkillKind::ChannelBurst:
        case sim::SkillKind::TerrainRush:
        case sim::SkillKind::RadialSlow:
            return sim::SkillTargetMode::Self;
        case sim::SkillKind::LowHealthPassive:
            return sim::SkillTargetMode::Passive;
        }
        return sim::SkillTargetMode::Self;
    };

    for (const sim::HeroDef *hero : heroes) {
        godot::Dictionary hero_data;
        hero_data["id"] = hero->Id;
        hero_data["name"] = godot::String(hero->Name.c_str());
        hero_data["role"] = godot::String(hero->Role.c_str());
        hero_data["description"] =
            godot::String(hero->Description.c_str());
        hero_data["prefab_id"] = hero->PrefabId;
        hero_data["base_hp"] = hero->BaseHp;
        hero_data["base_mana"] = hero->BaseMana;
        hero_data["base_attack"] = hero->BaseAtk;
        hero_data["base_attack_speed"] = hero->BaseAsp;
        hero_data["base_move_speed"] = hero->BaseMoveSpeed;
        hero_data["attack_range"] = hero->AttackRange;
        hero_data["attack_type"] =
            hero->AttackType == sim::AttackDelivery::Melee ? "melee"
                                                           : "projectile";

        godot::Array skills;
        for (int skill_id : hero->SkillIds) {
            auto it = config.Skills.find(skill_id);
            if (it == config.Skills.end())
                continue;
            const auto &skill = it->second;
            godot::Dictionary skill_data;
            skill_data["id"] = skill.Id;
            skill_data["name"] = godot::String(skill.Name.c_str());
            skill_data["description"] =
                godot::String(skill.Description.c_str());
            skill_data["target_mode"] =
                static_cast<int>(target_mode(skill.Kind));
            skill_data["max_level"] = skill.MaxLevel;
            skill_data["is_passive"] =
                target_mode(skill.Kind) == sim::SkillTargetMode::Passive;
            skills.push_back(skill_data);
        }
        hero_data["skills"] = skills;
        catalog.push_back(hero_data);
    }
    return catalog;
}

// ── v2 新命令 API ──
void SimServer::set_skill_command(int slot, bool confirm, float aim_x, float aim_y, int target_id) {
    _world.set_skill_command(slot, confirm, aim_x, aim_y, target_id);
}

void SimServer::set_skill_upgrade_command(int slot) {
    _world.set_skill_upgrade_command(slot);
}

void SimServer::set_attack_command_full(int target_id, bool ground, float gx, float gy, bool clear) {
    _world.set_attack_command(target_id, ground, gx, gy, clear);
}

void SimServer::set_cancel_command(bool skill, bool attack) {
    _world.set_cancel_command(skill, attack);
}

void SimServer::set_move_command(float target_x, float target_y, bool issue) {
    _world.set_move_command(target_x, target_y, issue);
}

void SimServer::set_stop_command(bool stop) {
    _world.set_stop_command(stop);
}

// ── v1 旧 API（deprecated） ──
void SimServer::set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim,
                                 bool fire, int seq) {
    _world.set_local_input(
        sim::Vec2{static_cast<float>(move.x), static_cast<float>(move.y)},
        sim::Vec2{static_cast<float>(aim.x), static_cast<float>(aim.y)},
        fire, seq);
}

void SimServer::set_cast_input(int cast_slot, bool confirm, bool cancel,
                                bool interrupt, float aim_x, float aim_y, int target_id) {
    _world.set_cast_input(cast_slot, confirm, cancel, interrupt, aim_x, aim_y, target_id);
}

void SimServer::set_attack_command(int target_id) {
    _world.set_attack_command(target_id, false, 0.0f, 0.0f, false);
}

void SimServer::tick(double delta) {
    _world.tick(delta);
}

bool SimServer::is_game_over() {
    return _world.is_game_over();
}

int SimServer::get_hero_capacity() const {
    return _world.hero_capacity();
}

godot::Dictionary SimServer::get_perf_stats() const {
    const auto stats = _world.perf_stats();
    godot::Dictionary out;
    out["sample_ticks"] = static_cast<int64_t>(stats.Timing.SampleTicks);
    out["tick_avg_us"] = stats.Timing.TickAverageMicros;
    out["tick_p95_us"] = stats.Timing.TickP95Micros;
    out["tick_p99_us"] = stats.Timing.TickP99Micros;
    out["tick_max_us"] = static_cast<int64_t>(stats.Timing.TickMaxMicros);

    const char *phase_names[sim::perf_phase_count] = {
        "input_ai",
        "actions",
        "navigation",
        "movement_collision",
        "effects",
        "snapshot",
        "flush",
    };
    godot::Dictionary phases;
    for (size_t i = 0; i < sim::perf_phase_count; ++i) {
        godot::Dictionary phase;
        phase["avg_us"] = stats.Timing.PhaseAverageMicros[i];
        phase["p95_us"] = stats.Timing.PhaseP95Micros[i];
        phase["p99_us"] = stats.Timing.PhaseP99Micros[i];
        phase["max_us"] = static_cast<int64_t>(
            stats.Timing.PhaseMaxMicros[i]
        );
        phases[phase_names[i]] = phase;
    }
    out["phases"] = phases;

    godot::Dictionary navigation;
    navigation["submitted"] = static_cast<int64_t>(stats.Navigation.Submitted);
    navigation["rejected"] = static_cast<int64_t>(stats.Navigation.Rejected);
    navigation["completed"] = static_cast<int64_t>(stats.Navigation.Completed);
    navigation["no_path"] = static_cast<int64_t>(stats.Navigation.NoPath);
    navigation["merged"] = static_cast<int64_t>(stats.Navigation.Merged);
    navigation["stale"] = static_cast<int64_t>(stats.Navigation.Stale);
    navigation["late"] = static_cast<int64_t>(stats.Navigation.Late);
    navigation["result_age_ticks_total"] = static_cast<int64_t>(
        stats.Navigation.ResultAgeTicks
    );
    navigation["result_age_avg_ticks"] = stats.Navigation.Completed > 0
                                              ? static_cast<double>(
                                                    stats.Navigation.ResultAgeTicks
                                                ) /
                                                    stats.Navigation.Completed
                                              : 0.0;
    navigation["result_age_max_ticks"] = static_cast<int64_t>(
        stats.Navigation.ResultAgeMaxTicks
    );
    navigation["expanded_nodes"] = static_cast<int64_t>(
        stats.Navigation.ExpandedNodes
    );
    navigation["worker_us"] = static_cast<int64_t>(
        stats.Navigation.WorkerMicros
    );
    navigation["pending"] = static_cast<int64_t>(stats.Navigation.Pending);
    out["navigation"] = navigation;
    out["job_queued"] = static_cast<int64_t>(stats.JobQueued);
    out["job_peak_queued"] = static_cast<int64_t>(stats.JobPeakQueued);
    out["job_active"] = static_cast<int64_t>(stats.JobActive);
    return out;
}

godot::Ref<godot::RefCounted> SimServer::pop_snapshot() {
    return _world.pop_snapshot();
}
