#pragma once

#include "sim_skill_slot_snap.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace sim {

class SimPlayerSnap : public godot::RefCounted {
    GDCLASS(SimPlayerSnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0, speed = 0;
    int kills = 0, level = 0, xp = 0, xp_needed = 0;
    int cast_state = 0;
    int cast_slot = -1;
    float cast_progress = 0.0f;
    float cast_aim_x = 0.0f, cast_aim_y = 0.0f;
    float dash_sx = 0.0f, dash_sy = 0.0f;
    float dash_tx = 0.0f, dash_ty = 0.0f;
    int status = 0;
    int hit_target_id = -1;
    int cast_error = 0;
    int attack_target_id = -1;
    int cast_target_id = -1;
    bool is_moving = false;
    int skill_points = 0;
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
    float get_mana() const { return mana; }
    void set_mana(float v) { mana = v; }
    float get_max_mana() const { return max_mana; }
    void set_max_mana(float v) { max_mana = v; }
    float get_atk() const { return atk; }
    void set_atk(float v) { atk = v; }
    float get_asp() const { return asp; }
    void set_asp(float v) { asp = v; }
    float get_speed() const { return speed; }
    void set_speed(float v) { speed = v; }
    int get_kills() const { return kills; }
    void set_kills(int v) { kills = v; }
    int get_level() const { return level; }
    void set_level(int v) { level = v; }
    int get_xp() const { return xp; }
    void set_xp(int v) { xp = v; }
    int get_xp_needed() const { return xp_needed; }
    void set_xp_needed(int v) { xp_needed = v; }
    int get_cast_state() const { return cast_state; }
    void set_cast_state(int v) { cast_state = v; }
    int get_cast_slot() const { return cast_slot; }
    void set_cast_slot(int v) { cast_slot = v; }
    float get_cast_progress() const { return cast_progress; }
    void set_cast_progress(float v) { cast_progress = v; }
    float get_cast_aim_x() const { return cast_aim_x; }
    void set_cast_aim_x(float v) { cast_aim_x = v; }
    float get_cast_aim_y() const { return cast_aim_y; }
    void set_cast_aim_y(float v) { cast_aim_y = v; }
    float get_dash_sx() const { return dash_sx; }
    void set_dash_sx(float v) { dash_sx = v; }
    float get_dash_sy() const { return dash_sy; }
    void set_dash_sy(float v) { dash_sy = v; }
    float get_dash_tx() const { return dash_tx; }
    void set_dash_tx(float v) { dash_tx = v; }
    float get_dash_ty() const { return dash_ty; }
    void set_dash_ty(float v) { dash_ty = v; }
    int get_status() const { return status; }
    void set_status(int v) { status = v; }
    int get_hit_target_id() const { return hit_target_id; }
    void set_hit_target_id(int v) { hit_target_id = v; }
    int get_cast_error() const { return cast_error; }
    void set_cast_error(int v) { cast_error = v; }
    int get_attack_target_id() const { return attack_target_id; }
    void set_attack_target_id(int v) { attack_target_id = v; }
    int get_cast_target_id() const { return cast_target_id; }
    void set_cast_target_id(int v) { cast_target_id = v; }
    bool get_is_moving() const { return is_moving; }
    void set_is_moving(bool v) { is_moving = v; }
    int get_skill_points() const { return skill_points; }
    void set_skill_points(int v) { skill_points = v; }
    godot::TypedArray<SimSkillSlotSnap> get_skills() const { return skills; }
    void set_skills(const godot::TypedArray<SimSkillSlotSnap> &v) {
        skills = v;
    }

  protected:
    static void _bind_methods();
};

} // namespace sim