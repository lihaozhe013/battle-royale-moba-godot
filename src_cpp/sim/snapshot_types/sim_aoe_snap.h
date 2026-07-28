#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace sim {

class SimAoESnap : public godot::RefCounted {
    GDCLASS(SimAoESnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0;
    float radius = 0.0f;
    float remaining = 0.0f;
    float duration = 0.0f;

    int get_id() const { return id; }
    void set_id(int v) { id = v; }
    float get_x() const { return x; }
    void set_x(float v) { x = v; }
    float get_y() const { return y; }
    void set_y(float v) { y = v; }
    float get_radius() const { return radius; }
    void set_radius(float v) { radius = v; }
    float get_remaining() const { return remaining; }
    void set_remaining(float v) { remaining = v; }
    float get_duration() const { return duration; }
    void set_duration(float v) { duration = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim