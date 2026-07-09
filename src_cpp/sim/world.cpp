#include "world.h"
#include "game_config.h"
#include "json_util.h"
#include "skill_defs.h"
#include <cmath>

namespace sim {

World::World() : _rng(42) {}

void World::initialize(const std::string &map_json) {
    auto map = parse_map_json(map_json);

    _map_bounds_entity = _reg.create();
    _reg.emplace<MapBounds>(_map_bounds_entity, map.half);

    _local_input_entity = _reg.create();
    _reg.emplace<LocalInputSingleton>(
        _local_input_entity,
        Vec2{0.0f},
        Vec2{0.0f},
        false,
        0,
        -1,
        false,
        false,
        false,
        Vec2{0.0f},
        -1,
        Vec2{0.0f},
        false,
        false
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

void World::set_local_input(
    const Vec2 &move, const Vec2 &aim, bool fire, int seq
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.Move = move;
        li.Aim = aim;
        li.Fire = fire;
        li.Seq = seq;
    }
}

void World::set_cast_input(
    int cast_slot,
    bool confirm,
    bool cancel,
    bool interrupt,
    float aim_x,
    float aim_y,
    int target_id
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.CastSlot = cast_slot;
        li.CastConfirm = confirm;
        li.CastCancel = cancel;
        li.CastInterrupt = interrupt;
        li.CastAim = Vec2{aim_x, aim_y};
        li.CastTargetId = target_id;
    }
}

void World::set_move_command(float target_x, float target_y, bool issue) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.MoveTarget = Vec2{target_x, target_y};
        li.MoveIssue = issue;
    }
}

void World::set_stop(bool stop) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.Stop = stop;
    }
}

void World::tick(double dt) {
    if (_game_over)
        return;

    _time += dt;
    float fdt = static_cast<float>(dt);

    local_input_injection_system(_reg, _local_input_entity);

    player_pathfinding_system(_reg, _nav_grid);

    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    player_movement_system(_reg, fdt, map_half);

    auto &ids = _reg.get<IdState>(_id_state_entity);
    player_fire_system(_reg, _time, _cb, ids);

    skill_cast_system(_reg, fdt, _cb, ids, _time);
    bot_targeting_system(_reg, _rng, fdt);
    bot_ai_system(_reg, fdt, map_half, _rng);
    bot_combat_system(_reg, _time, _cb, ids);
    arrow_movement_system(_reg, fdt);
    wall_collision_system(_reg, _cb);
    combat_system(_reg, _cb);

    // Check if local player died → stop game loop
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
    progression_system(_reg);
    snapshot_export_system(_reg, _tick_counter, _latest_snapshot);

    _cb.flush(_reg);
}

void World::_spawn_player(int player_id, bool is_local) {
    auto &ids = _reg.get<IdState>(_id_state_entity);
    ids.NextPlayerId = player_id + 1;

    float half = GameConfig::MapHalf - GameConfig::PlayerRadius;
    Vec2 pos = _random_map_pos(half, GameConfig::PlayerRadius);

    auto e = _reg.create();
    _reg.emplace<PlayerTag>(e, is_local);
    _reg.emplace<NetworkId>(e, player_id);
    _reg.emplace<Position2D>(e, pos);
    _reg.emplace<FacingAngle>(e, 0.0f);
    _reg.emplace<Health>(e, GameConfig::PlayerBaseHp, GameConfig::PlayerBaseHp);
    _reg.emplace<Mana>(
        e,
        GameConfig::PlayerBaseMana,
        GameConfig::PlayerBaseMana,
        GameConfig::PlayerManaRegen,
        GameConfig::ManaRegenDelay,
        0.0f
    );
    _reg.emplace<CombatStats>(
        e, GameConfig::BaseAttack, GameConfig::BaseAttackSpeed, 0.0
    );
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<PlayerInputState>(
        e,
        Vec2{0.0f},
        Vec2{0.0f},
        false,
        0,
        -1,
        false,
        false,
        false,
        Vec2{0.0f},
        -1,
        Vec2{0.0f},
        false,
        false
    );
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, 1);
    _reg.emplace<Experience>(e, 0, GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, GameConfig::PlayerSpeed);
    _reg.emplace<CastState>(e);
    _reg.emplace<StatusEffect>(e);
    _reg.emplace<MovePath>(e);

    SkillComponent sc;
    for (int i = 0; i < 4; ++i) {
        int sid = GameConfig::PlayerSkillIds[i];
        const auto &def = get_skill_def(sid);
        sc.Slots[i].SkillId = sid;
        sc.Slots[i].MaxCooldown = def.Cooldown;
        sc.Slots[i].ManaCost = def.ManaCost;
    }
    _reg.emplace<SkillComponent>(e, sc);
}

void World::_spawn_bot() {
    int total_w = GameConfig::FodderWeight + GameConfig::StalkerWeight +
                  GameConfig::BruteWeight;
    int r = std::uniform_int_distribution<int>(0, total_w - 1)(_rng);
    BotRole role;
    if (r < GameConfig::FodderWeight)
        role = BotRole::Fodder;
    else if (r < GameConfig::FodderWeight + GameConfig::StalkerWeight)
        role = BotRole::Stalker;
    else
        role = BotRole::Brute;

    int new_lv;
    if (_count_high_level_bots() < 3) {
        new_lv = std::uniform_int_distribution<int>(
            25, GameConfig::MaxHeroLevel
        )(_rng);
    } else {
        int plv = _get_player_level();
        int offset = std::uniform_int_distribution<int>(-3, 3)(_rng);
        new_lv = std::clamp(plv + offset, 1, GameConfig::MaxHeroLevel);
    }
    _spawn_bot_with_role(role, new_lv);
}

void World::_spawn_bot_with_role(BotRole role, int new_lv) {
    auto &ids = _reg.get<IdState>(_id_state_entity);
    int bot_id = ids.NextBotId++;
    BotTier tier = detail::roll_bot_tier_for_role(role, _rng);
    auto mult = detail::tier_mult(tier);

    int base_hp = GameConfig::BotHp + (new_lv - 1) * GameConfig::HpPerLevel;
    float atk =
        (GameConfig::BotBaseAttack + (new_lv - 1) * GameConfig::AtkPerLevel) *
        mult.AtkMul;
    float asp = std::min(
        (GameConfig::BotBaseAttackSpeed + (new_lv - 1) * GameConfig::AspPerLevel
        ) * mult.AspMul,
        GameConfig::AspMax
    );
    float spd =
        (GameConfig::BotSpeed + (new_lv - 1) * GameConfig::SpeedPerLevel) *
        mult.SpeedMul;
    float vis = GameConfig::BotVisionRange * mult.VisionMul;

    float half = GameConfig::MapHalf - GameConfig::BotRadius;
    Vec2 pos = _random_map_pos(half, GameConfig::BotRadius);
    Vec2 target = _random_map_pos(half, GameConfig::BotRadius);

    auto e = _reg.create();
    _reg.emplace<BotTag>(e);
    _reg.emplace<NetworkId>(e, bot_id);
    _reg.emplace<Position2D>(e, pos);
    _reg.emplace<FacingAngle>(e, 0.0f);
    _reg.emplace<Health>(
        e,
        static_cast<int>(base_hp * mult.HpMul),
        static_cast<int>(base_hp * mult.HpMul)
    );
    _reg.emplace<Mana>(
        e,
        GameConfig::BotBaseMana,
        GameConfig::BotBaseMana,
        GameConfig::BotManaRegen,
        GameConfig::ManaRegenDelay,
        0.0f
    );
    _reg.emplace<BotAIState>(
        e, target, 0.0f, entt::null, _random_wander_time()
    );
    _reg.emplace<BotBehaviorState>(e);
    _reg.emplace<BotTier>(e, tier);
    _reg.emplace<BotRole>(e, role);
    _reg.emplace<BotVisionRange>(e, vis);
    _reg.emplace<CombatStats>(e, atk, asp, 0.0);
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, new_lv);
    _reg.emplace<Experience>(e, 0, new_lv * GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, spd);
    _reg.emplace<StatusEffect>(e);

    SkillComponent sc;
    for (int i = 0; i < 4; ++i) {
        int sid = GameConfig::BotSkillIds[i];
        const auto &def = get_skill_def(sid);
        sc.Slots[i].SkillId = sid;
        sc.Slots[i].MaxCooldown = def.Cooldown;
        sc.Slots[i].ManaCost = def.ManaCost;
    }
    _reg.emplace<SkillComponent>(e, sc);
}

void World::_spawn_pickup_spawners() {
    // Collect wall bounds for spawn position validation
    std::vector<WallBounds> walls;
    auto wall_view = _reg.view<WallBounds>();
    for (auto w : wall_view)
        walls.push_back(_reg.get<WallBounds>(w));
    float half = _reg.get<MapBounds>(_map_bounds_entity).Half;

    struct SpawnDef {
        PickupType type;
        int value;
        Vec2 pos;
        float respawn;
    };
    std::uniform_real_distribution<float> xp_offset(-20.0f, 20.0f);
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 12; ++col) {
            float base_x = -44.0f + col * 8.0f;
            float base_y = -36.0f + row * 8.0f;
            Vec2 pos{base_x + xp_offset(_rng), base_y + xp_offset(_rng)};

            // Discard if outside map bounds or inside a wall
            if (std::abs(pos.x) >= half || std::abs(pos.y) >= half)
                continue;
            bool blocked = false;
            for (auto &w : walls) {
                if (point_inside_aabb(pos, w.Min, w.Max)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;

            _spawn_one_spawner(
                PickupType::Xp,
                GameConfig::XpPickupValue,
                pos,
                GameConfig::XpPickupRespawnTime
            );
        }
    }
    SpawnDef heal[] = {
        {PickupType::Heal,
         GameConfig::HealPickupValue,
         Vec2{-20, -20},
         GameConfig::HealPickupRespawnTime},
        {PickupType::Heal,
         GameConfig::HealPickupValue,
         Vec2{20, 20},
         GameConfig::HealPickupRespawnTime},
    };
    for (auto &s : heal)
        _spawn_one_spawner(s.type, s.value, s.pos, s.respawn);
    SpawnDef small[] = {
        {PickupType::SmallHeal,
         GameConfig::SmallHealPickupValue,
         Vec2{-10, 10},
         GameConfig::SmallHealPickupRespawnTime},
        {PickupType::SmallHeal,
         GameConfig::SmallHealPickupValue,
         Vec2{10, -10},
         GameConfig::SmallHealPickupRespawnTime},
    };
    for (auto &s : small)
        _spawn_one_spawner(s.type, s.value, s.pos, s.respawn);
}

void World::_spawn_one_spawner(
    PickupType type, int value, Vec2 pos, float respawn_time
) {
    auto e = _reg.create();
    _reg.emplace<PickupSpawner>(
        e, type, value, pos, respawn_time, respawn_time * 0.5f, false, 0
    );
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
