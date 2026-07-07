#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace sim {

class SimSkillSlotSnap : public godot::RefCounted {
    GDCLASS(SimSkillSlotSnap, godot::RefCounted)
public:
    int skill_id = 0;
    int level = 0;
    float cooldown = 0.0f;
    float max_cooldown = 0.0f;
    float mana_cost = 0.0f;

    int get_skill_id() const { return skill_id; } void set_skill_id(int v) { skill_id = v; }
    int get_level() const { return level; } void set_level(int v) { level = v; }
    float get_cooldown() const { return cooldown; } void set_cooldown(float v) { cooldown = v; }
    float get_max_cooldown() const { return max_cooldown; } void set_max_cooldown(float v) { max_cooldown = v; }
    float get_mana_cost() const { return mana_cost; } void set_mana_cost(float v) { mana_cost = v; }
protected:
    static void _bind_methods();
};

class SimEventSnap : public godot::RefCounted {
    GDCLASS(SimEventSnap, godot::RefCounted)
public:
    int killer_id = 0;
    int victim_id = 0;
    int get_killer_id() const { return killer_id; }
    void set_killer_id(int v) { killer_id = v; }
    int get_victim_id() const { return victim_id; }
    void set_victim_id(int v) { victim_id = v; }
protected:
    static void _bind_methods();
};

class SimPlayerSnap : public godot::RefCounted {
    GDCLASS(SimPlayerSnap, godot::RefCounted)
public:
    int id = 0; float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0, speed = 0;
    int kills = 0, level = 0, xp = 0, xp_needed = 0;
    godot::TypedArray<SimSkillSlotSnap> skills;

    int get_id() const { return id; } void set_id(int v) { id = v; }
    float get_x() const { return x; } void set_x(float v) { x = v; }
    float get_y() const { return y; } void set_y(float v) { y = v; }
    float get_ang() const { return ang; } void set_ang(float v) { ang = v; }
    int get_hp() const { return hp; } void set_hp(int v) { hp = v; }
    int get_max_hp() const { return max_hp; } void set_max_hp(int v) { max_hp = v; }
    float get_mana() const { return mana; } void set_mana(float v) { mana = v; }
    float get_max_mana() const { return max_mana; } void set_max_mana(float v) { max_mana = v; }
    float get_atk() const { return atk; } void set_atk(float v) { atk = v; }
    float get_asp() const { return asp; } void set_asp(float v) { asp = v; }
    float get_speed() const { return speed; } void set_speed(float v) { speed = v; }
    int get_kills() const { return kills; } void set_kills(int v) { kills = v; }
    int get_level() const { return level; } void set_level(int v) { level = v; }
    int get_xp() const { return xp; } void set_xp(int v) { xp = v; }
    int get_xp_needed() const { return xp_needed; } void set_xp_needed(int v) { xp_needed = v; }
    godot::TypedArray<SimSkillSlotSnap> get_skills() const { return skills; }
    void set_skills(const godot::TypedArray<SimSkillSlotSnap> &v) { skills = v; }
protected:
    static void _bind_methods();
};

class SimBotSnap : public godot::RefCounted {
    GDCLASS(SimBotSnap, godot::RefCounted)
public:
    int id = 0; float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0; bool dead = false;
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0; int kills = 0; int level = 0;
    int xp = 0, xp_needed = 0;
    float speed = 0;
    int tier = 0;
    godot::TypedArray<SimSkillSlotSnap> skills;

    int get_id() const { return id; } void set_id(int v) { id = v; }
    float get_x() const { return x; } void set_x(float v) { x = v; }
    float get_y() const { return y; } void set_y(float v) { y = v; }
    float get_ang() const { return ang; } void set_ang(float v) { ang = v; }
    int get_hp() const { return hp; } void set_hp(int v) { hp = v; }
    int get_max_hp() const { return max_hp; } void set_max_hp(int v) { max_hp = v; }
    bool get_dead() const { return dead; } void set_dead(bool v) { dead = v; }
    float get_mana() const { return mana; } void set_mana(float v) { mana = v; }
    float get_max_mana() const { return max_mana; } void set_max_mana(float v) { max_mana = v; }
    float get_atk() const { return atk; } void set_atk(float v) { atk = v; }
    float get_asp() const { return asp; } void set_asp(float v) { asp = v; }
    int get_kills() const { return kills; } void set_kills(int v) { kills = v; }
    int get_level() const { return level; } void set_level(int v) { level = v; }
    int get_xp() const { return xp; } void set_xp(int v) { xp = v; }
    int get_xp_needed() const { return xp_needed; } void set_xp_needed(int v) { xp_needed = v; }
    float get_speed() const { return speed; } void set_speed(float v) { speed = v; }
    int get_tier() const { return tier; } void set_tier(int v) { tier = v; }
    godot::TypedArray<SimSkillSlotSnap> get_skills() const { return skills; }
    void set_skills(const godot::TypedArray<SimSkillSlotSnap> &v) { skills = v; }
protected:
    static void _bind_methods();
};

class SimArrowSnap : public godot::RefCounted {
    GDCLASS(SimArrowSnap, godot::RefCounted)
public:
    int id = 0; float x = 0, y = 0, ang = 0;
    int get_id() const { return id; } void set_id(int v) { id = v; }
    float get_x() const { return x; } void set_x(float v) { x = v; }
    float get_y() const { return y; } void set_y(float v) { y = v; }
    float get_ang() const { return ang; } void set_ang(float v) { ang = v; }
protected:
    static void _bind_methods();
};

class SimPickupSnap : public godot::RefCounted {
    GDCLASS(SimPickupSnap, godot::RefCounted)
public:
    int id = 0; float x = 0, y = 0;
    int type = 0; int value = 0;
    int get_id() const { return id; } void set_id(int v) { id = v; }
    float get_x() const { return x; } void set_x(float v) { x = v; }
    float get_y() const { return y; } void set_y(float v) { y = v; }
    int get_type() const { return type; } void set_type(int v) { type = v; }
    int get_value() const { return value; } void set_value(int v) { value = v; }
protected:
    static void _bind_methods();
};

class SimSnapshot : public godot::RefCounted {
    GDCLASS(SimSnapshot, godot::RefCounted)
public:
    int seq = 0;
    int64_t t = 0;
    godot::TypedArray<SimPlayerSnap> players;
    godot::TypedArray<SimBotSnap> bots;
    godot::TypedArray<SimArrowSnap> arrows;
    godot::TypedArray<SimPickupSnap> pickups;
    godot::TypedArray<SimEventSnap> events;

    int get_seq() const { return seq; } void set_seq(int v) { seq = v; }
    int64_t get_t() const { return t; } void set_t(int64_t v) { t = v; }
    godot::TypedArray<SimPlayerSnap> get_players() const { return players; }
    void set_players(const godot::TypedArray<SimPlayerSnap> &v) { players = v; }
    godot::TypedArray<SimBotSnap> get_bots() const { return bots; }
    void set_bots(const godot::TypedArray<SimBotSnap> &v) { bots = v; }
    godot::TypedArray<SimArrowSnap> get_arrows() const { return arrows; }
    void set_arrows(const godot::TypedArray<SimArrowSnap> &v) { arrows = v; }
    godot::TypedArray<SimPickupSnap> get_pickups() const { return pickups; }
    void set_pickups(const godot::TypedArray<SimPickupSnap> &v) { pickups = v; }
    godot::TypedArray<SimEventSnap> get_events() const { return events; }
    void set_events(const godot::TypedArray<SimEventSnap> &v) { events = v; }
protected:
    static void _bind_methods();
};

} // namespace sim
