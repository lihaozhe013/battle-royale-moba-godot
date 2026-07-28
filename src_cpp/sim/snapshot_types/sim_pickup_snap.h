#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace sim {

class SimPickupSnap : public godot::RefCounted {
    GDCLASS(SimPickupSnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0;
    int type = 0;
    int value = 0;
    int get_id() const { return id; }
    void set_id(int v) { id = v; }
    float get_x() const { return x; }
    void set_x(float v) { x = v; }
    float get_y() const { return y; }
    void set_y(float v) { y = v; }
    int get_type() const { return type; }
    void set_type(int v) { type = v; }
    int get_value() const { return value; }
    void set_value(int v) { value = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim