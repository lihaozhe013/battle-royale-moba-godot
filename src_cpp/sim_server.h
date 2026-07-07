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
    void set_skill_input(bool q, bool w, bool e, bool r);
    void tick(double delta);
    godot::Ref<godot::RefCounted> pop_snapshot();

private:
    sim::World _world;
};
