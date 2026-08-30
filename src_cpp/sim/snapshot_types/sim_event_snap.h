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
    int damage = 0;
    int healing = 0;
    bool critical = false;
    int get_type() const { return type; }
    void set_type(int v) { type = v; }
    int get_killer_id() const { return killer_id; }
    void set_killer_id(int v) { killer_id = v; }
    int get_victim_id() const { return victim_id; }
    void set_victim_id(int v) { victim_id = v; }
    int get_source_skill_id() const { return source_skill_id; }
    void set_source_skill_id(int v) { source_skill_id = v; }
    int get_damage() const { return damage; }
    void set_damage(int v) { damage = v; }
    int get_healing() const { return healing; }
    void set_healing(int v) { healing = v; }
    bool get_critical() const { return critical; }
    void set_critical(bool v) { critical = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim
