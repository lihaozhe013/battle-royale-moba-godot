#include "sim_server.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

SimServer::SimServer() {
}

SimServer::~SimServer() {
}

void SimServer::_bind_methods() {
    godot::ClassDB::bind_method(
        godot::D_METHOD("initialize", "map_json"),
        &SimServer::initialize
    );
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_local_input", "move", "aim", "fire", "seq"),
        &SimServer::set_local_input
    );
    godot::ClassDB::bind_method(
        godot::D_METHOD("tick", "delta"),
        &SimServer::tick
    );
    godot::ClassDB::bind_method(
        godot::D_METHOD("pop_snapshot"),
        &SimServer::pop_snapshot
    );
}

void SimServer::initialize(const godot::String &p_map_json) {
    godot::UtilityFunctions::print("SimServer::initialize called");
    _world.initialize(p_map_json.utf8().get_data());
}

void SimServer::set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim, bool fire, int seq) {
    _world.set_local_input(
        sim::Vec2{static_cast<float>(move.x), static_cast<float>(move.y)},
        sim::Vec2{static_cast<float>(aim.x), static_cast<float>(aim.y)},
        fire, seq
    );
}

void SimServer::tick(double delta) {
    _world.tick(delta);
}

godot::Ref<godot::RefCounted> SimServer::pop_snapshot() {
    return godot::Ref<godot::RefCounted>();
}
