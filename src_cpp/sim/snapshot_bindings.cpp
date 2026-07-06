#include "snapshot_types.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

#define BIND(cls, field) \
    ClassDB::bind_method(D_METHOD("get_" #field), &cls::get_##field); \
    ClassDB::bind_method(D_METHOD("set_" #field, "v"), &cls::set_##field);

#define PROP(cls, type, field) \
    ADD_PROPERTY(PropertyInfo(type, #field), "set_" #field, "get_" #field);

namespace sim {

void SimEventSnap::_bind_methods() {
    BIND(SimEventSnap, killer_id);
    BIND(SimEventSnap, victim_id);
    PROP(SimEventSnap, Variant::INT, killer_id);
    PROP(SimEventSnap, Variant::INT, victim_id);
}

void SimPlayerSnap::_bind_methods() {
    BIND(SimPlayerSnap, id); BIND(SimPlayerSnap, x); BIND(SimPlayerSnap, y);
    BIND(SimPlayerSnap, ang); BIND(SimPlayerSnap, hp); BIND(SimPlayerSnap, max_hp);
    BIND(SimPlayerSnap, atk); BIND(SimPlayerSnap, asp); BIND(SimPlayerSnap, speed);
    BIND(SimPlayerSnap, kills); BIND(SimPlayerSnap, level);
    BIND(SimPlayerSnap, xp); BIND(SimPlayerSnap, xp_needed);

    PROP(SimPlayerSnap, Variant::INT, id);
    PROP(SimPlayerSnap, Variant::FLOAT, x);
    PROP(SimPlayerSnap, Variant::FLOAT, y);
    PROP(SimPlayerSnap, Variant::FLOAT, ang);
    PROP(SimPlayerSnap, Variant::INT, hp);
    PROP(SimPlayerSnap, Variant::INT, max_hp);
    PROP(SimPlayerSnap, Variant::FLOAT, atk);
    PROP(SimPlayerSnap, Variant::FLOAT, asp);
    PROP(SimPlayerSnap, Variant::FLOAT, speed);
    PROP(SimPlayerSnap, Variant::INT, kills);
    PROP(SimPlayerSnap, Variant::INT, level);
    PROP(SimPlayerSnap, Variant::INT, xp);
    PROP(SimPlayerSnap, Variant::INT, xp_needed);
}

void SimBotSnap::_bind_methods() {
    BIND(SimBotSnap, id); BIND(SimBotSnap, x); BIND(SimBotSnap, y);
    BIND(SimBotSnap, ang); BIND(SimBotSnap, hp); BIND(SimBotSnap, max_hp);
    BIND(SimBotSnap, dead);
    BIND(SimBotSnap, atk); BIND(SimBotSnap, asp);
    BIND(SimBotSnap, kills); BIND(SimBotSnap, level);
    BIND(SimBotSnap, xp); BIND(SimBotSnap, xp_needed);
    BIND(SimBotSnap, speed); BIND(SimBotSnap, tier);

    PROP(SimBotSnap, Variant::INT, id);
    PROP(SimBotSnap, Variant::FLOAT, x);
    PROP(SimBotSnap, Variant::FLOAT, y);
    PROP(SimBotSnap, Variant::FLOAT, ang);
    PROP(SimBotSnap, Variant::INT, hp);
    PROP(SimBotSnap, Variant::INT, max_hp);
    PROP(SimBotSnap, Variant::BOOL, dead);
    PROP(SimBotSnap, Variant::FLOAT, atk);
    PROP(SimBotSnap, Variant::FLOAT, asp);
    PROP(SimBotSnap, Variant::INT, kills);
    PROP(SimBotSnap, Variant::INT, level);
    PROP(SimBotSnap, Variant::INT, xp);
    PROP(SimBotSnap, Variant::INT, xp_needed);
    PROP(SimBotSnap, Variant::FLOAT, speed);
    PROP(SimBotSnap, Variant::INT, tier);
}

void SimArrowSnap::_bind_methods() {
    BIND(SimArrowSnap, id); BIND(SimArrowSnap, x); BIND(SimArrowSnap, y);
    BIND(SimArrowSnap, ang);

    PROP(SimArrowSnap, Variant::INT, id);
    PROP(SimArrowSnap, Variant::FLOAT, x);
    PROP(SimArrowSnap, Variant::FLOAT, y);
    PROP(SimArrowSnap, Variant::FLOAT, ang);
}

void SimPickupSnap::_bind_methods() {
    BIND(SimPickupSnap, id); BIND(SimPickupSnap, x); BIND(SimPickupSnap, y);
    BIND(SimPickupSnap, type); BIND(SimPickupSnap, value);

    PROP(SimPickupSnap, Variant::INT, id);
    PROP(SimPickupSnap, Variant::FLOAT, x);
    PROP(SimPickupSnap, Variant::FLOAT, y);
    PROP(SimPickupSnap, Variant::INT, type);
    PROP(SimPickupSnap, Variant::INT, value);
}

void SimSnapshot::_bind_methods() {
    BIND(SimSnapshot, seq);
    ClassDB::bind_method(D_METHOD("get_t"), &SimSnapshot::get_t);
    ClassDB::bind_method(D_METHOD("set_t", "v"), &SimSnapshot::set_t);
    ClassDB::bind_method(D_METHOD("get_players"), &SimSnapshot::get_players);
    ClassDB::bind_method(D_METHOD("set_players", "v"), &SimSnapshot::set_players);
    ClassDB::bind_method(D_METHOD("get_bots"), &SimSnapshot::get_bots);
    ClassDB::bind_method(D_METHOD("set_bots", "v"), &SimSnapshot::set_bots);
    ClassDB::bind_method(D_METHOD("get_arrows"), &SimSnapshot::get_arrows);
    ClassDB::bind_method(D_METHOD("set_arrows", "v"), &SimSnapshot::set_arrows);
    ClassDB::bind_method(D_METHOD("get_pickups"), &SimSnapshot::get_pickups);
    ClassDB::bind_method(D_METHOD("set_pickups", "v"), &SimSnapshot::set_pickups);
    ClassDB::bind_method(D_METHOD("get_events"), &SimSnapshot::get_events);
    ClassDB::bind_method(D_METHOD("set_events", "v"), &SimSnapshot::set_events);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "seq"), "set_seq", "get_seq");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "t"), "set_t", "get_t");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "players", PROPERTY_HINT_ARRAY_TYPE, "SimPlayerSnap"), "set_players", "get_players");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bots", PROPERTY_HINT_ARRAY_TYPE, "SimBotSnap"), "set_bots", "get_bots");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "arrows", PROPERTY_HINT_ARRAY_TYPE, "SimArrowSnap"), "set_arrows", "get_arrows");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "pickups", PROPERTY_HINT_ARRAY_TYPE, "SimPickupSnap"), "set_pickups", "get_pickups");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "events", PROPERTY_HINT_ARRAY_TYPE, "SimEventSnap"), "set_events", "get_events");
}

} // namespace sim
