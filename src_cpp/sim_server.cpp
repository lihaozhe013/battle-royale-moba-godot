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
    _time = 0.0;
}

void SimServer::set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim, bool fire, int seq) {
}

void SimServer::tick(double delta) {
    _time += delta;
}

godot::Ref<godot::RefCounted> SimServer::pop_snapshot() {
    return godot::Ref<godot::RefCounted>();
}
