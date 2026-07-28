#include "world.h"
#include "game_config.h"
#include "heroes/hero_registry.h"
#include "json_util.h"
#include <cmath>

namespace sim {

World::World() : _rng(42) {
    register_builtin_heroes();
    register_builtin_skills();
}

void World::initialize(const std::string &map_json) {
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
        GameConfig::PlayerIdStart,
        GameConfig::BotIdStart,
        GameConfig::ArrowIdStart,
        GameConfig::PickupIdStart,
        GameConfig::AoEIdStart
    );

    _kill_event_entity = _reg.create();
    _reg.emplace<KillEventBuffer>(_kill_event_entity);

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

    for (int i = 0; i < GameConfig::BotCount; ++i)
        _spawn_bot();
    _spawn_player(GameConfig::PlayerIdStart, true);
    _spawn_pickup_spawners();
}

void World::tick(double dt) {
    if (_game_over)
        return;

    _time += dt;
    float fdt = static_cast<float>(dt);

    auto &ids = _reg.get<IdState>(_id_state_entity);
    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;

    local_input_injection_system(_reg, _local_input_entity);

    bot_targeting_system(_reg, _rng, fdt);
    bot_ai_system(_reg, fdt, map_half, _rng);
    bot_combat_state_system(_reg, fdt);
    bot_skill_decider_system(_reg, _rng);
    bot_input_injection_system(_reg);

    attack_command_system(_reg, fdt);
    skill_cast_system(_reg, fdt, _cb, ids, _time);
    pathfinding_system(_reg, _nav_grid);
    movement_system(_reg, fdt, map_half);
    attack_fire_system(_reg, _time, _cb, ids);

    arrow_movement_system(_reg, fdt);
    wall_collision_system(_reg, _cb);
    combat_system(_reg, _cb);

    {
        auto pv = _reg.view<PlayerTag, Dead>();
        for (auto p : pv) {
            if (pv.get<PlayerTag>(p).IsLocal && pv.get<Dead>(p).enabled) {
                _game_over = true;
                return;
            }
        }
    }

    pickup_system(_reg, fdt, _cb, ids);
    aoe_system(_reg, fdt, _cb);
    status_effect_system(_reg, fdt);
    mana_regen_system(_reg, fdt);
    skill_cooldown_system(_reg, fdt);
    skill_level_system(_reg);
    progression_system(_reg);
    snapshot_export_system(_reg, _tick_counter, _latest_snapshot);

    _cb.flush(_reg);
}

Vec2 World::_random_map_pos(float half, float radius) {
    std::uniform_real_distribution<float> dist(-half, half);
    return Vec2{dist(_rng), dist(_rng)};
}

float World::_random_wander_time() {
    std::uniform_real_distribution<float> dist(
        GameConfig::BotWanderIntervalMin, GameConfig::BotWanderIntervalMax
    );
    return dist(_rng);
}

void World::_build_nav_grid() {
    std::vector<WallBounds> walls;
    auto wall_view = _reg.view<WallBounds>();
    for (auto w : wall_view)
        walls.push_back(_reg.get<WallBounds>(w));

    float half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    _nav_grid.build(half, walls, 0.5f, GameConfig::PlayerRadius);
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