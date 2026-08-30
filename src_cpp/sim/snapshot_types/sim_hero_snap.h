#pragma once

#include "sim_skill_slot_snap.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace sim {

class SimHeroSnap : public godot::RefCounted {
    GDCLASS(SimHeroSnap, godot::RefCounted)
  public:
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    bool dead = false;
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0, speed = 0;
    float attack_range = 0.0f;
    int kills = 0, level = 0, xp = 0, xp_needed = 0;
    int status = 0;
    godot::TypedArray<SimSkillSlotSnap> skills;
    int deaths = 0;
    int damage_dealt = 0;
    int damage_taken = 0;
    int healing_done = 0;
    int xp_earned = 0;
    int skill_casts = 0;
    int score = 0;

    int cast_state = 0;
    int cast_slot = -1;
    float cast_progress = 0.0f;
    float cast_aim_x = 0.0f, cast_aim_y = 0.0f;
    float dash_sx = 0.0f, dash_sy = 0.0f;
    float dash_tx = 0.0f, dash_ty = 0.0f;
    int hit_target_id = -1;
    int cast_error = 0;
    int attack_target_id = -1;
    int cast_target_id = -1;
    bool is_moving = false;
    int skill_points = 0;

    int tier = 0;
    bool is_local = false;
    int hero_def_id = 0;
    godot::String hero_name;
    int prefab_id = 0;

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
    float get_speed() const { return speed; }
    void set_speed(float v) { speed = v; }
    float get_attack_range() const { return attack_range; }
    void set_attack_range(float v) { attack_range = v; }
    int get_kills() const { return kills; }
    void set_kills(int v) { kills = v; }
    int get_level() const { return level; }
    void set_level(int v) { level = v; }
    int get_xp() const { return xp; }
    void set_xp(int v) { xp = v; }
    int get_xp_needed() const { return xp_needed; }
    void set_xp_needed(int v) { xp_needed = v; }
    int get_status() const { return status; }
    void set_status(int v) { status = v; }
    godot::TypedArray<SimSkillSlotSnap> get_skills() const { return skills; }
    void set_skills(const godot::TypedArray<SimSkillSlotSnap> &v) {
        skills = v;
    }
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
    int get_tier() const { return tier; }
    void set_tier(int v) { tier = v; }
    bool get_is_local() const { return is_local; }
    void set_is_local(bool v) { is_local = v; }
    int get_hero_def_id() const { return hero_def_id; }
    void set_hero_def_id(int v) { hero_def_id = v; }
    godot::String get_hero_name() const { return hero_name; }
    void set_hero_name(const godot::String &v) { hero_name = v; }
    int get_prefab_id() const { return prefab_id; }
    void set_prefab_id(int v) { prefab_id = v; }
    int get_deaths() const { return deaths; }
    void set_deaths(int v) { deaths = v; }
    int get_damage_dealt() const { return damage_dealt; }
    void set_damage_dealt(int v) { damage_dealt = v; }
    int get_damage_taken() const { return damage_taken; }
    void set_damage_taken(int v) { damage_taken = v; }
    int get_healing_done() const { return healing_done; }
    void set_healing_done(int v) { healing_done = v; }
    int get_xp_earned() const { return xp_earned; }
    void set_xp_earned(int v) { xp_earned = v; }
    int get_skill_casts() const { return skill_casts; }
    void set_skill_casts(int v) { skill_casts = v; }
    int get_score() const { return score; }
    void set_score(int v) { score = v; }

  protected:
    static void _bind_methods();
};

} // namespace sim
