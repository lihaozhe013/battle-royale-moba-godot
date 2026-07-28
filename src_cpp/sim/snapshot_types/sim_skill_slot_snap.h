#pragma once

#include <godot_cpp/classes/ref_counted.hpp>

namespace sim {

class SimSkillSlotSnap : public godot::RefCounted {
    GDCLASS(SimSkillSlotSnap, godot::RefCounted)
  public:
    int skill_id = 0;
    int level = 0;
    float cooldown = 0.0f;
    float max_cooldown = 0.0f;
    float mana_cost = 0.0f;

    int get_skill_id() const { return skill_id; }
    void set_skill_id(int v) { skill_id = v; }
    int get_level() const { return level; }
    void set_level(int v) { level = v; }
    float get_cooldown() const { return cooldown; }
    void set_cooldown(float v) { cooldown = v; }
    float get_max_cooldown() const { return max_cooldown; }
    void set_max_cooldown(float v) { max_cooldown = v; }
    float get_mana_cost() const { return mana_cost; }
    void set_mana_cost(float v) { mana_cost = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim