#pragma once

#include <entt/entt.hpp>
#include <random>
#include <string>
#include "components.h"
#include "vec2.h"
#include "command_buffer.h"
#include "systems/local_input_injection.h"
#include "systems/player_movement.h"
#include "systems/player_fire.h"
#include "systems/bot_targeting.h"
#include "systems/bot_ai.h"
#include "systems/bot_combat.h"
#include "systems/arrow_movement.h"
#include "systems/wall_collision.h"
#include "systems/combat.h"
#include "systems/pickup.h"
#include "systems/progression.h"
#include "systems/snapshot_export.h"

namespace sim {

class World {
public:
    World();

    void initialize(const std::string &map_json);
    void set_local_input(const Vec2 &move, const Vec2 &aim, bool fire, int seq);
    void tick(double dt);

    entt::registry &registry() { return _reg; }
    const entt::registry &registry() const { return _reg; }

    CommandBuffer &commands() { return _cb; }

    entt::entity local_input_entity() const { return _local_input_entity; }
    entt::entity map_bounds_entity() const { return _map_bounds_entity; }
    entt::entity id_state_entity() const { return _id_state_entity; }
    entt::entity kill_event_entity() const { return _kill_event_entity; }

    double time() const { return _time; }
    std::mt19937 &rng() { return _rng; }

private:
    void _spawn_player(int player_id, bool is_local);
    void _spawn_bot();
    void _spawn_pickup_spawners();
    void _spawn_one_spawner(PickupType type, int value, Vec2 pos, float respawn_time);
    Vec2 _random_map_pos(float half, float radius);
    float _random_wander_time();

    entt::registry _reg;
    CommandBuffer _cb;
    double _time = 0.0;
    std::mt19937 _rng{42};

    entt::entity _local_input_entity = entt::null;
    entt::entity _map_bounds_entity = entt::null;
    entt::entity _id_state_entity = entt::null;
    entt::entity _kill_event_entity = entt::null;
};

} // namespace sim
