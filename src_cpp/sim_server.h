#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>
#include "sim/world.h"

class SimServer : public godot::RefCounted {
    GDCLASS(SimServer, godot::RefCounted)

protected:
    static void _bind_methods();

public:
    SimServer();
    ~SimServer();

    void initialize(const godot::String &map_json);
    void set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim, bool fire, int seq);
    void set_cast_input(int cast_slot, bool confirm, bool cancel, bool interrupt, float aim_x, float aim_y, int target_id = -1);
    void set_move_command(float target_x, float target_y, bool issue);
    void set_stop(bool stop);
    void set_attack_command(int target_id);
    void tick(double delta);
    bool is_game_over();
    godot::Ref<godot::RefCounted> pop_snapshot();

private:
    sim::World _world;
};
