#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace sim {

class SimEventSnap : public godot::RefCounted {
    GDCLASS(SimEventSnap, godot::RefCounted)
  public:
    int type = 0;
    int killer_id = 0;
    int victim_id = 0;
    int source_skill_id = 0;
    int get_type() const { return type; }
    void set_type(int v) { type = v; }
    int get_killer_id() const { return killer_id; }
    void set_killer_id(int v) { killer_id = v; }
    int get_victim_id() const { return victim_id; }
    void set_victim_id(int v) { victim_id = v; }
    int get_source_skill_id() const { return source_skill_id; }
    void set_source_skill_id(int v) { source_skill_id = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim
