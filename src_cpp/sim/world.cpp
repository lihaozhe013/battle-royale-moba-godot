#include "world.h"
#include "game_config.h"
#include "json_util.h"
#include <cmath>

namespace sim {

World::World()
    : _rng(42)
{
}

void World::initialize(const std::string &map_json) {
    auto map = parse_map_json(map_json);

    // MapBounds singleton
    _map_bounds_entity = _reg.create();
    _reg.emplace<MapBounds>(_map_bounds_entity, map.half);

    // LocalInputSingleton
    _local_input_entity = _reg.create();
    _reg.emplace<LocalInputSingleton>(_local_input_entity,
        Vec2{0.0f}, Vec2{0.0f}, false, 0);

    // IdState
    _id_state_entity = _reg.create();
    _reg.emplace<IdState>(_id_state_entity,
        GameConfig::PlayerIdStart,
        GameConfig::BotIdStart,
        GameConfig::ArrowIdStart,
        GameConfig::PickupIdStart
    );

    // KillEvent buffer
    _kill_event_entity = _reg.create();
    _reg.emplace<KillEventBuffer>(_kill_event_entity);

    // Walls
    for (auto &w : map.walls) {
        float min_x = std::min(w.minX, w.maxX);
        float max_x = std::max(w.minX, w.maxX);
        float min_y = std::min(w.minY, w.maxY);
        float max_y = std::max(w.minY, w.maxY);
        auto wall = _reg.create();
        _reg.emplace<WallTag>(wall);
        _reg.emplace<WallBounds>(wall, Vec2{min_x, min_y}, Vec2{max_x, max_y});
    }

    // Bots
    for (int i = 0; i < GameConfig::BotCount; ++i) {
        _spawn_bot();
    }

    // Player
    _spawn_player(GameConfig::PlayerIdStart, true);

    // Pickup spawners
    _spawn_pickup_spawners();
}

void World::set_local_input(const Vec2 &move, const Vec2 &aim, bool fire, int seq) {
    if (_local_input_entity != entt::null) {
        _reg.replace<LocalInputSingleton>(_local_input_entity,
            move, aim, fire, seq);
    }
}

void World::tick(double dt) {
    _time += dt;
    float fdt = static_cast<float>(dt);

    // 1. LocalInputInjectionSystem
    local_input_injection_system(_reg, _local_input_entity);

    // 2. PlayerMovementSystem
    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    player_movement_system(_reg, fdt, map_half);

    // 3. PlayerFireSystem
    auto &ids = _reg.get<IdState>(_id_state_entity);
    player_fire_system(_reg, _time, _cb, ids);

    // 4. BotTargetingSystem
    bot_targeting_system(_reg, _rng, fdt);

    // 5. BotAISystem
    bot_ai_system(_reg, fdt, map_half, _rng);

    // 6. BotCombatSystem
    bot_combat_system(_reg, _time, _cb, ids);

    // 7. ArrowMovementSystem
    arrow_movement_system(_reg, fdt);

    // 8. WallCollisionSystem
    wall_collision_system(_reg, _cb);

    // 9. CombatSystem
    combat_system(_reg, _cb);

    // 10. PickupSystem
    pickup_system(_reg, fdt, _cb, ids);

    // 11. ProgressionSystem
    progression_system(_reg);

    // 12. SnapshotExportSystem
    snapshot_export_system(_reg, _tick_counter, _latest_snapshot);

    // Flush deferred structural changes (matches Unity ECB behavior)
    _cb.flush(_reg);
}

// ── private ──────────────────────────────────────────────────────────────

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
    _reg.emplace<CombatStats>(e, GameConfig::BaseAttack, GameConfig::BaseAttackSpeed, 0.0);
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<PlayerInputState>(e, Vec2{0.0f}, Vec2{0.0f}, false, 0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, 1);
    _reg.emplace<Experience>(e, 0, GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, GameConfig::PlayerSpeed);
}

void World::_spawn_bot() {
    auto &ids = _reg.get<IdState>(_id_state_entity);
    int bot_id = ids.NextBotId++;

    float half = GameConfig::MapHalf - GameConfig::BotRadius;
    Vec2 pos = _random_map_pos(half, GameConfig::BotRadius);
    Vec2 target = _random_map_pos(half, GameConfig::BotRadius);

    // Random level 1~30 + tier roll (same as respawn logic in bot_ai.h)
    int new_lv = std::uniform_int_distribution<int>(1, GameConfig::MaxBotLevel)(_rng);
    float r = std::uniform_real_distribution<float>(0.0f, 1.0f)(_rng);
    BotTier tier;
    float hp_mul, atk_mul, asp_mul, speed_mul, vision_mul;
    if (r < GameConfig::BossRoll) {
        tier = BotTier::Boss;
        hp_mul = GameConfig::BossHpMul; atk_mul = GameConfig::BossAtkMul;
        asp_mul = GameConfig::BossAspMul; speed_mul = GameConfig::BossSpeedMul;
        vision_mul = GameConfig::BossVisionMul;
    } else if (r < GameConfig::EliteRoll) {
        tier = BotTier::Elite;
        hp_mul = GameConfig::EliteHpMul; atk_mul = GameConfig::EliteAtkMul;
        asp_mul = GameConfig::EliteAspMul; speed_mul = GameConfig::EliteSpeedMul;
        vision_mul = GameConfig::EliteVisionMul;
    } else {
        tier = BotTier::Normal;
        hp_mul = GameConfig::NormalHpMul; atk_mul = GameConfig::NormalAtkMul;
        asp_mul = GameConfig::NormalAspMul; speed_mul = GameConfig::NormalSpeedMul;
        vision_mul = GameConfig::NormalVisionMul;
    }

    int base_hp = GameConfig::BotHp + (new_lv - 1) * GameConfig::BotHpPerLevel;
    float atk = (GameConfig::BotBaseAttack + (new_lv - 1) * GameConfig::BotAtkPerLevel) * atk_mul;
    float asp = std::min(
        (GameConfig::BotBaseAttackSpeed + (new_lv - 1) * GameConfig::BotAspPerLevel) * asp_mul,
        GameConfig::AspMax);
    float spd = (GameConfig::BotSpeed + (new_lv - 1) * GameConfig::BotSpeedPerLevel) * speed_mul;
    float vis = GameConfig::BotVisionRange * vision_mul;

    auto e = _reg.create();
    _reg.emplace<BotTag>(e);
    _reg.emplace<NetworkId>(e, bot_id);
    _reg.emplace<Position2D>(e, pos);
    _reg.emplace<FacingAngle>(e, 0.0f);
    _reg.emplace<Health>(e, static_cast<int>(base_hp * hp_mul), static_cast<int>(base_hp * hp_mul));
    _reg.emplace<BotAIState>(e, target, 0.0f, entt::null, _random_wander_time());
    _reg.emplace<BotBehaviorState>(e);
    _reg.emplace<BotTier>(e, tier);
    _reg.emplace<BotVisionRange>(e, vis);
    _reg.emplace<CombatStats>(e, atk, asp, 0.0);
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, new_lv);
    _reg.emplace<Experience>(e, 0, new_lv * GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, spd);
}

void World::_spawn_pickup_spawners() {
    struct SpawnDef { PickupType type; int value; Vec2 pos; float respawn; };

    SpawnDef xp_spawners[] = {
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-35, -35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-17.5, -35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{   0, -35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{ 17.5, -35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{  35, -35}, GameConfig::XpPickupRespawnTime},

        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-35, -17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-17.5, -17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{   0, -17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{ 17.5, -17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{  35, -17.5}, GameConfig::XpPickupRespawnTime},

        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-35,    0}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-17.5,  0}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{ 17.5,  0}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{  35,   0}, GameConfig::XpPickupRespawnTime},

        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-35, 17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-17.5, 17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{   0, 17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{ 17.5, 17.5}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{  35, 17.5}, GameConfig::XpPickupRespawnTime},

        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-35, 35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{-17.5, 35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{   0, 35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{ 17.5, 35}, GameConfig::XpPickupRespawnTime},
        {PickupType::Xp, GameConfig::XpPickupValue, Vec2{  35, 35}, GameConfig::XpPickupRespawnTime},
    };
    for (auto &s : xp_spawners) {
        _spawn_one_spawner(s.type, s.value, s.pos, s.respawn);
    }

    SpawnDef heal_spawners[] = {
        {PickupType::Heal, GameConfig::HealPickupValue, Vec2{-20, -20}, GameConfig::HealPickupRespawnTime},
        {PickupType::Heal, GameConfig::HealPickupValue, Vec2{ 20,  20}, GameConfig::HealPickupRespawnTime},
    };
    for (auto &s : heal_spawners) {
        _spawn_one_spawner(s.type, s.value, s.pos, s.respawn);
    }

    SpawnDef small_heal_spawners[] = {
        {PickupType::SmallHeal, GameConfig::SmallHealPickupValue, Vec2{-10, 10}, GameConfig::SmallHealPickupRespawnTime},
        {PickupType::SmallHeal, GameConfig::SmallHealPickupValue, Vec2{ 10,-10}, GameConfig::SmallHealPickupRespawnTime},
    };
    for (auto &s : small_heal_spawners) {
        _spawn_one_spawner(s.type, s.value, s.pos, s.respawn);
    }
}

void World::_spawn_one_spawner(PickupType type, int value, Vec2 pos, float respawn_time) {
    auto &ids = _reg.get<IdState>(_id_state_entity);

    auto e = _reg.create();
    _reg.emplace<PickupSpawner>(e,
        type, value, pos, respawn_time,
        respawn_time * 0.5f, false, 0
    );
}

Vec2 World::_random_map_pos(float half, float radius) {
    std::uniform_real_distribution<float> dist(-half, half);
    return Vec2{dist(_rng), dist(_rng)};
}

float World::_random_wander_time() {
    std::uniform_real_distribution<float> dist(
        GameConfig::BotWanderIntervalMin,
        GameConfig::BotWanderIntervalMax
    );
    return dist(_rng);
}

} // namespace sim
