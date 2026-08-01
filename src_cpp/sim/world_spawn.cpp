#include "game_config.h"
#include "heroes/hero_registry.h"
#include "skills/skill_registry.h"
#include "world.h"
#include <cmath>

namespace sim {

void World::_spawn_player(int player_id, bool is_local) {
    auto &ids = _reg.get<IdState>(_id_state_entity);
    ids.NextPlayerId = player_id + 1;

    const auto &def = HeroRegistry::instance().get(1);

    float half = GameConfig::MapHalf - GameConfig::PlayerRadius;
    Vec2 pos = _random_map_pos(half, GameConfig::PlayerRadius);

    auto e = _reg.create();
    _reg.emplace<HeroTag>(e, is_local);
    _reg.emplace<HeroDefId>(e, def.Id);
    _reg.emplace<NetworkId>(e, player_id);
    _reg.emplace<Position2D>(e, pos);
    _reg.emplace<FacingAngle>(e, 0.0f);
    _reg.emplace<Health>(e, def.BaseHp, def.BaseHp);
    _reg.emplace<Mana>(
        e,
        def.BaseMana,
        def.BaseMana,
        GameConfig::PlayerManaRegen,
        GameConfig::ManaRegenDelay,
        0.0f
    );
    _reg.emplace<CombatStats>(e, def.BaseAtk, def.BaseAsp, -999.0);
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<HeroInputState>(e);
    _reg.emplace<SkillPoints>(e, 0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, 1);
    _reg.emplace<Experience>(e, 0, GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, def.BaseMoveSpeed);
    _reg.emplace<CastState>(e);
    _reg.emplace<StatusEffect>(e);
    _reg.emplace<MovePath>(e);
    _reg.emplace<AttackTarget>(e);

    SkillComponent sc;
    for (int i = 0; i < 4; ++i) {
        int sid = def.SkillIds[i];
        const ISkill *sk = SkillRegistry::instance().get(sid);
        sc.Slots[i].SkillId = sid;
        sc.Slots[i].Level = 1;
        sc.Slots[i].MaxCooldown = sk ? sk->base_cooldown() : 0.0f;
        sc.Slots[i].ManaCost = sk ? sk->base_mana_cost() : 0.0f;
    }
    _reg.emplace<SkillComponent>(e, sc);

    auto bot_view = _reg.view<BotTag, Position2D>();
    for (auto b : bot_view) {
        Vec2 &bp = bot_view.get<Position2D>(b).Value;
        Vec2 delta = bp - pos;
        if (vec2_length_sq(delta) <
            GameConfig::PlayerSpawnSafeRadiusSq) {
            bp = _random_map_pos(half, GameConfig::BotRadius);
        }
    }
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
        new_lv =
            std::uniform_int_distribution<int>(25, GameConfig::MaxHeroLevel)(
                _rng
            );
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

    const auto &def = HeroRegistry::instance().get(1);

    int base_hp = GameConfig::BotHp + (new_lv - 1) * GameConfig::HpPerLevel;
    float atk =
        (GameConfig::BotBaseAttack + (new_lv - 1) * GameConfig::AtkPerLevel) *
        mult.AtkMul * GameConfig::BotStatMul;
    float asp = std::min(
        (GameConfig::BotBaseAttackSpeed +
         (new_lv - 1) * GameConfig::AspPerLevel) *
            mult.AspMul * GameConfig::BotStatMul,
        GameConfig::AspMax
    );
    float spd =
        (GameConfig::BotSpeed + (new_lv - 1) * GameConfig::SpeedPerLevel) *
        mult.SpeedMul;
    float vis =
        GameConfig::BotVisionRange * mult.VisionMul * GameConfig::BotStatMul;

    float half = GameConfig::MapHalf - GameConfig::BotRadius;
    Vec2 pos = _random_map_pos(half, GameConfig::BotRadius);
    Vec2 target = _random_map_pos(half, GameConfig::BotRadius);

    auto e = _reg.create();
    _reg.emplace<HeroTag>(e, false);
    _reg.emplace<HeroDefId>(e, def.Id);
    _reg.emplace<BotTag>(e);
    _reg.emplace<NetworkId>(e, bot_id);
    _reg.emplace<Position2D>(e, pos);
    _reg.emplace<FacingAngle>(e, 0.0f);
    _reg.emplace<Health>(
        e,
        static_cast<int>(base_hp * mult.HpMul * GameConfig::BotStatMul),
        static_cast<int>(base_hp * mult.HpMul * GameConfig::BotStatMul)
    );
    _reg.emplace<Mana>(
        e,
        GameConfig::BotBaseMana,
        GameConfig::BotBaseMana,
        GameConfig::BotManaRegen,
        GameConfig::ManaRegenDelay,
        0.0f
    );
    _reg.emplace<HeroInputState>(e);
    _reg.emplace<BotAIState>(
        e, target, 0.0f, entt::null, _random_wander_time()
    );
    _reg.emplace<BotBehaviorState>(e);
    _reg.emplace<BotTier>(e, tier);
    _reg.emplace<BotRole>(e, role);
    _reg.emplace<BotVisionRange>(e, vis);
    _reg.emplace<CombatStats>(e, atk, asp, -999.0);
    _reg.emplace<Kills>(e, 0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, new_lv);
    _reg.emplace<Experience>(e, 0, new_lv * GameConfig::XpPerLevelBase);
    _reg.emplace<MoveSpeed>(e, spd);
    _reg.emplace<CastState>(e);
    _reg.emplace<StatusEffect>(e);
    _reg.emplace<MovePath>(e);
    _reg.emplace<AttackTarget>(e);
    _reg.emplace<BotCombatState>(e);

    SkillComponent sc;
    for (int i = 0; i < 4; ++i) {
        int sid = def.SkillIds[i];
        const ISkill *sk = SkillRegistry::instance().get(sid);
        sc.Slots[i].SkillId = sid;
        sc.Slots[i].Level = 1;
        sc.Slots[i].MaxCooldown =
            sk ? sk->base_cooldown() * GameConfig::BotSkillCooldownMul : 0.0f;
        sc.Slots[i].ManaCost = sk ? sk->base_mana_cost() : 0.0f;
    }
    _reg.emplace<SkillComponent>(e, sc);
}

void World::_spawn_pickup_spawners() {
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

} // namespace sim