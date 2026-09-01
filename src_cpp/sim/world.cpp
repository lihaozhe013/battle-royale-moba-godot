#include "world.h"
#include "game_config.h"
#include "heroes/hero_registry.h"
#include "json_util.h"
#include <cmath>

namespace sim {

World::World() : _jobs(), _navigation(_jobs), _rng(42) {}

bool World::initialize(
    const std::string &map_json,
    const std::string &stats_yaml,
    int local_hero_id
) {
    _last_error.clear();
    _jobs.wait_idle();
    _navigation.clear();
    _navigation.reset_metrics();
    _nav_grid.reset();
    _wall_bounds.clear();
    _reg.clear();
    _reg.ctx().clear();
    _cb = CommandBuffer{};
    _rng.seed(42);
    _time = 0.0;
    _tick_counter = 0;
    _snapshot_accumulator = 0.0;
    _has_snapshot = false;
    _perf.reset();
    _game_over = false;
    _local_input_entity = entt::null;
    _map_bounds_entity = entt::null;
    _id_state_entity = entt::null;
    _kill_event_entity = entt::null;
    _local_hero_id = local_hero_id;
    _latest_snapshot = godot::Ref<SimSnapshot>();
    StatsConfig config;
    std::string stats_error;
    if (!load_stats_yaml(stats_yaml, config, stats_error)) {
        _last_error = stats_error;
        return false;
    }
    _jobs.reconfigure(
        config.JobWorkerThreads > 0
            ? static_cast<size_t>(config.JobWorkerThreads)
            : 0
    );
    _reg.ctx().emplace<StatsConfig>(std::move(config));
    register_builtin_heroes(stats(_reg));
    register_builtin_skills(stats(_reg));

    const HeroDef *local_hero = HeroRegistry::instance().find(_local_hero_id);
    if (!local_hero) {
        _last_error =
            "unknown local hero id: " + std::to_string(_local_hero_id);
        return false;
    }
    for (int skill_id : local_hero->SkillIds) {
        if (!SkillRegistry::instance().has(skill_id)) {
            _last_error =
                "hero references unregistered skill id: " +
                std::to_string(skill_id);
            return false;
        }
    }
    const HeroDef *bot_hero = HeroRegistry::instance().find(1);
    if (!bot_hero) {
        _last_error = "stats.yaml must define bot hero id 1 (Swordsman)";
        return false;
    }
    for (int skill_id : bot_hero->SkillIds) {
        if (!SkillRegistry::instance().has(skill_id)) {
            _last_error =
                "bot hero references unregistered skill id: " +
                std::to_string(skill_id);
            return false;
        }
    }

    auto map = parse_map_json(map_json);

    _map_bounds_entity = _reg.create();
    _reg.emplace<MapBounds>(_map_bounds_entity, map.half);

    _local_input_entity = _reg.create();
    _reg.emplace<LocalInputSingleton>(
        _local_input_entity,
        Vec2{0.0f},
        false,
        false,
        -1,
        false,
        Vec2{0.0f},
        -1,
        -1,
        false,
        false,
        -1,
        false,
        Vec2{0.0f},
        false,
        0
    );

    _id_state_entity = _reg.create();
    _reg.emplace<IdState>(
        _id_state_entity,
        stats(_reg).PlayerIdStart,
        stats(_reg).BotIdStart,
        stats(_reg).ArrowIdStart,
        stats(_reg).PickupIdStart,
        stats(_reg).AoEIdStart
    );

    _kill_event_entity = _reg.create();
    _reg.emplace<KillEventBuffer>(_kill_event_entity);
    _reg.emplace<ImpactEventBuffer>(_kill_event_entity);
    _reg.emplace<AttackStartedEventBuffer>(_kill_event_entity);

    for (auto &w : map.walls) {
        float min_x = std::min(w.minX, w.maxX);
        float max_x = std::max(w.minX, w.maxX);
        float min_y = std::min(w.minY, w.maxY);
        float max_y = std::max(w.minY, w.maxY);
        auto wall = _reg.create();
        _reg.emplace<WallTag>(wall);
        _reg.emplace<WallBounds>(wall, Vec2{min_x, min_y}, Vec2{max_x, max_y});
    }

    _build_nav_grid();

    for (int i = 0; i < stats(_reg).BotCount; ++i)
        _spawn_bot();
    _spawn_player(stats(_reg).PlayerIdStart, true);
    _spawn_pickup_spawners();
    return true;
}

int World::hero_capacity() const {
    if (!_reg.ctx().contains<StatsConfig>())
        return 0;
    return 1 + _reg.ctx().get<StatsConfig>().BotCount;
}

RuntimePerfStats World::perf_stats() const {
    RuntimePerfStats out;
    out.Timing = _perf.snapshot();
    out.Navigation = _navigation.metrics();
    out.JobQueued = _jobs.queued_jobs();
    out.JobPeakQueued = _jobs.peak_queued_jobs();
    out.JobActive = _jobs.active_jobs();
    return out;
}

void World::tick(double dt) {
    if (_game_over)
        return;

    _perf.begin_tick();
    _time += dt;
    float fdt = static_cast<float>(dt);

    auto &ids = _reg.get<IdState>(_id_state_entity);
    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;

    _perf.begin_phase(PerfPhase::InputAi);
    timed_modifier_system(_reg, fdt);
    local_input_injection_system(_reg, _local_input_entity);

    bot_targeting_system(_reg, _rng, fdt);
    bot_ai_system(_reg, fdt, map_half, _rng);
    bot_combat_state_system(_reg, fdt);
    bot_skill_decider_system(_reg, fdt);
    bot_input_injection_system(_reg);
    _perf.end_phase();

    _perf.begin_phase(PerfPhase::Actions);
    attack_command_system(_reg, fdt);
    skill_cast_system(_reg, fdt, _cb, ids, _time);
    _perf.end_phase();

    _perf.begin_phase(PerfPhase::Navigation);
    pathfinding_system(
        _reg,
        _navigation,
        _tick_counter + 1,
        stats(_reg).PathMaxSubmissionsPerTick,
        stats(_reg).PathFailureRetryTicks
    );
    _perf.end_phase();

    _perf.begin_phase(PerfPhase::MovementCollision);
    movement_system(_reg, fdt, map_half);
    attack_fire_system(_reg, _time, _cb, ids, _rng);

    arrow_movement_system(_reg, fdt);
    wall_collision_system(_reg, _cb, &_wall_bounds);
    combat_system(_reg, _cb);
    _perf.end_phase();

    _perf.begin_phase(PerfPhase::Effects);
    pickup_system(_reg, fdt, _cb, ids);
    aoe_system(_reg, fdt, _cb);
    status_effect_system(_reg, fdt);
    mana_regen_system(_reg, fdt);
    skill_cooldown_system(_reg, fdt);
    skill_level_system(_reg);
    progression_system(_reg);
    _perf.end_phase();

    auto pv = _reg.view<PlayerTag, Dead>();
    for (auto p : pv) {
        if (pv.get<PlayerTag>(p).IsLocal && pv.get<Dead>(p).enabled) {
            _game_over = true;
            break;
        }
    }

    _perf.begin_phase(PerfPhase::Snapshot);
    _snapshot_accumulator += dt;
    const double snapshot_interval =
        1.0 / std::max(1.0f, stats(_reg).SnapshotRate);
    bool emit_snapshot = !_has_snapshot ||
                         _snapshot_accumulator >= snapshot_interval ||
                         _game_over;
    if (emit_snapshot && _has_snapshot) {
        while (_snapshot_accumulator >= snapshot_interval)
            _snapshot_accumulator -= snapshot_interval;
    } else if (emit_snapshot) {
        _snapshot_accumulator = 0.0;
    }
    _has_snapshot = _has_snapshot || emit_snapshot;

    snapshot_export_system(
        _reg,
        _tick_counter,
        _time,
        _game_over ? static_cast<int>(MatchResult::Defeat)
                   : static_cast<int>(MatchResult::InProgress),
        _latest_snapshot,
        emit_snapshot
    );
    _perf.end_phase();

    _perf.begin_phase(PerfPhase::Flush);
    auto impact_view = _reg.view<ImpactEventBuffer>();
    for (auto e : impact_view)
        impact_view.get<ImpactEventBuffer>(e).events.clear();
    auto attack_started_view = _reg.view<AttackStartedEventBuffer>();
    for (auto e : attack_started_view)
        attack_started_view.get<AttackStartedEventBuffer>(e).events.clear();

    _cb.flush(_reg);
    _perf.end_phase();
    _perf.end_tick();
}

Vec2 World::_random_map_pos(float half, float radius) {
    std::uniform_real_distribution<float> dist(-half, half);
    return Vec2{dist(_rng), dist(_rng)};
}

float World::_random_wander_time() {
    std::uniform_real_distribution<float> dist(
        stats(_reg).BotWanderIntervalMin, stats(_reg).BotWanderIntervalMax
    );
    return dist(_rng);
}

void World::_build_nav_grid() {
    _wall_bounds.clear();
    auto wall_view = _reg.view<WallBounds>();
    for (auto w : wall_view)
        _wall_bounds.push_back(_reg.get<WallBounds>(w));

    float half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    _nav_grid = std::make_shared<NavGrid>();
    _nav_grid->build(half, _wall_bounds, 0.5f, stats(_reg).PlayerRadius);
    _navigation.set_nav_grid(_nav_grid);
}

int World::_get_player_level() {
    auto pv = _reg.view<PlayerTag, Level>();
    for (auto p : pv) {
        if (pv.get<PlayerTag>(p).IsLocal)
            return pv.get<Level>(p).Value;
    }
    return 1;
}

int World::_count_high_level_bots() {
    int count = 0;
    auto bv = _reg.view<BotTag, Level>();
    for (auto b : bv) {
        if (bv.get<Level>(b).Value >= 25)
            count++;
    }
    return count;
}

} // namespace sim
