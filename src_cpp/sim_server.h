#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>

class SimServer : public godot::RefCounted {
    GDCLASS(SimServer, godot::RefCounted)

protected:
    static void _bind_methods();

public:
    SimServer();
    ~SimServer();

    void initialize(const godot::String &map_json);
    void set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim, bool fire, int seq);
    void tick(double delta);
    godot::Ref<godot::RefCounted> pop_snapshot();

private:
    double _time = 0.0;
};
