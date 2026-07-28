#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace sim {

class SimArrowSnap : public godot::RefCounted {
    GDCLASS(SimArrowSnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int get_id() const { return id; }
    void set_id(int v) { id = v; }
    float get_x() const { return x; }
    void set_x(float v) { x = v; }
    float get_y() const { return y; }
    void set_y(float v) { y = v; }
    float get_ang() const { return ang; }
    void set_ang(float v) { ang = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim