#pragma once

#include "sim_aoe_snap.h"
#include "sim_arrow_snap.h"
#include "sim_bot_snap.h"
#include "sim_event_snap.h"
#include "sim_hero_snap.h"
#include "sim_pickup_snap.h"
#include "sim_player_snap.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace sim {

class SimSnapshot : public godot::RefCounted {
    GDCLASS(SimSnapshot, godot::RefCounted)
  public:
    int seq = 0;
    int64_t t = 0;
    int result = 0;
    float match_time = 0.0f;
    godot::TypedArray<SimPlayerSnap> players;
    godot::TypedArray<SimBotSnap> bots;
    godot::TypedArray<SimHeroSnap> heroes;
    godot::TypedArray<SimArrowSnap> arrows;
    godot::TypedArray<SimPickupSnap> pickups;
    godot::TypedArray<SimEventSnap> events;
    godot::TypedArray<SimAoESnap> aoes;

    int get_seq() const { return seq; }
    void set_seq(int v) { seq = v; }
    int64_t get_t() const { return t; }
    void set_t(int64_t v) { t = v; }
    int get_result() const { return result; }
    void set_result(int v) { result = v; }
    float get_match_time() const { return match_time; }
    void set_match_time(float v) { match_time = v; }
    godot::TypedArray<SimPlayerSnap> get_players() const { return players; }
    void set_players(const godot::TypedArray<SimPlayerSnap> &v) { players = v; }
    godot::TypedArray<SimBotSnap> get_bots() const { return bots; }
    void set_bots(const godot::TypedArray<SimBotSnap> &v) { bots = v; }
    godot::TypedArray<SimHeroSnap> get_heroes() const { return heroes; }
    void set_heroes(const godot::TypedArray<SimHeroSnap> &v) { heroes = v; }
    godot::TypedArray<SimArrowSnap> get_arrows() const { return arrows; }
    void set_arrows(const godot::TypedArray<SimArrowSnap> &v) { arrows = v; }
    godot::TypedArray<SimPickupSnap> get_pickups() const { return pickups; }
    void set_pickups(const godot::TypedArray<SimPickupSnap> &v) { pickups = v; }
    godot::TypedArray<SimEventSnap> get_events() const { return events; }
    void set_events(const godot::TypedArray<SimEventSnap> &v) { events = v; }
    godot::TypedArray<SimAoESnap> get_aoes() const { return aoes; }
    void set_aoes(const godot::TypedArray<SimAoESnap> &v) { aoes = v; }

    int get_local_hero_index() const {
        for (int i = 0; i < heroes.size(); ++i) {
            auto h = Object::cast_to<SimHeroSnap>(heroes[i]);
            if (h && h->is_local)
                return i;
        }
        return -1;
    }

  protected:
    static void _bind_methods();
};

} // namespace sim
