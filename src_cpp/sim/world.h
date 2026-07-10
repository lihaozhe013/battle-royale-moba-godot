#pragma once

#include "command_buffer.h"
#include "components.h"
#include "nav_grid.h"
#include "systems/aoe.h"
#include "systems/arrow_movement.h"
#include "systems/bot_ai.h"
#include "systems/bot_combat.h"
#include "systems/bot_role_rules.h"
#include "systems/bot_targeting.h"
#include "systems/combat.h"
#include "systems/local_input_injection.h"
#include "systems/mana_regen.h"
#include "systems/pickup.h"
#include "systems/player_attack_command.h"
#include "systems/player_attack_fire.h"
#include "systems/player_movement.h"
#include "systems/player_pathfinding.h"
#include "systems/progression.h"
#include "systems/skill_cast.h"
#include "systems/skill_cooldown.h"
#include "systems/snapshot_export.h"
#include "systems/status_effect.h"
#include "systems/wall_collision.h"
#include "vec2.h"
#include <entt/entt.hpp>
#include <random>
#include <string>

namespace sim {

class World {
  public:
    World();

    void initialize(const std::string &map_json);
    void set_local_input(const Vec2 &move, const Vec2 &aim, bool fire, int seq);
    void set_cast_input(
        int cast_slot,
        bool confirm,
        bool cancel,
        bool interrupt,
        float aim_x,
        float aim_y,
        int target_id = -1
    );
    void set_move_command(float target_x, float target_y, bool issue);
    void set_stop(bool stop);
    void set_attack_command(int target_id, bool attack_ground,
                            float ground_x, float ground_y, bool attack_clear);
    void tick(double dt);
    bool is_game_over() const { return _game_over; }

    entt::registry &registry() { return _reg; }
    const entt::registry &registry() const { return _reg; }

    CommandBuffer &commands() { return _cb; }

    entt::entity local_input_entity() const { return _local_input_entity; }
    entt::entity map_bounds_entity() const { return _map_bounds_entity; }
    entt::entity id_state_entity() const { return _id_state_entity; }
    entt::entity kill_event_entity() const { return _kill_event_entity; }

    double time() const { return _time; }
    std::mt19937 &rng() { return _rng; }

    godot::Ref<SimSnapshot> pop_snapshot() {
        auto s = _latest_snapshot;
        _latest_snapshot = godot::Ref<SimSnapshot>();
        return s;
    }

  private:
    void _spawn_player(int player_id, bool is_local);
    void _spawn_bot();
    void _spawn_bot_with_role(BotRole role, int level);
    void _spawn_pickup_spawners();
    void _spawn_one_spawner(
        PickupType type, int value, Vec2 pos, float respawn_time
    );
    Vec2 _random_map_pos(float half, float radius);
    float _random_wander_time();
    void _build_nav_grid();
    int _get_player_level();
    int _count_high_level_bots();

    entt::registry _reg;
    NavGrid _nav_grid;
    CommandBuffer _cb;
    double _time = 0.0;
    int _tick_counter = 0;
    std::mt19937 _rng{42};
    godot::Ref<SimSnapshot> _latest_snapshot;

    entt::entity _local_input_entity = entt::null;
    entt::entity _map_bounds_entity = entt::null;
    entt::entity _id_state_entity = entt::null;
    entt::entity _kill_event_entity = entt::null;
    bool _game_over = false;
};

} // namespace sim
