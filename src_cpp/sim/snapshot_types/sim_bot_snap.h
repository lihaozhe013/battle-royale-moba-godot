#pragma once

#include "sim_skill_slot_snap.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace sim {

class SimBotSnap : public godot::RefCounted {
    GDCLASS(SimBotSnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    bool dead = false;
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0;
    int kills = 0;
    int level = 0;
    int xp = 0, xp_needed = 0;
    float speed = 0;
    int tier = 0;
    int status = 0;
    godot::TypedArray<SimSkillSlotSnap> skills;

    int get_id() const { return id; }
    void set_id(int v) { id = v; }
    float get_x() const { return x; }
    void set_x(float v) { x = v; }
    float get_y() const { return y; }
    void set_y(float v) { y = v; }
    float get_ang() const { return ang; }
    void set_ang(float v) { ang = v; }
    int get_hp() const { return hp; }
    void set_hp(int v) { hp = v; }
    int get_max_hp() const { return max_hp; }
    void set_max_hp(int v) { max_hp = v; }
    bool get_dead() const { return dead; }
    void set_dead(bool v) { dead = v; }
    float get_mana() const { return mana; }
    void set_mana(float v) { mana = v; }
    float get_max_mana() const { return max_mana; }
    void set_max_mana(float v) { max_mana = v; }
    float get_atk() const { return atk; }
    void set_atk(float v) { atk = v; }
    float get_asp() const { return asp; }
    void set_asp(float v) { asp = v; }
    int get_kills() const { return kills; }
    void set_kills(int v) { kills = v; }
    int get_level() const { return level; }
    void set_level(int v) { level = v; }
    int get_xp() const { return xp; }
    void set_xp(int v) { xp = v; }
    int get_xp_needed() const { return xp_needed; }
    void set_xp_needed(int v) { xp_needed = v; }
    float get_speed() const { return speed; }
    void set_speed(float v) { speed = v; }
    int get_tier() const { return tier; }
    void set_tier(int v) { tier = v; }
    int get_status() const { return status; }
    void set_status(int v) { status = v; }
    godot::TypedArray<SimSkillSlotSnap> get_skills() const { return skills; }
    void set_skills(const godot::TypedArray<SimSkillSlotSnap> &v) {
        skills = v;
    }

  protected:
    static void _bind_methods();
};

} // namespace sim