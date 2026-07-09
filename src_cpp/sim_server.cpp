#include "sim_server.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

SimServer::SimServer() {}
SimServer::~SimServer() {}

void SimServer::_bind_methods() {
    godot::ClassDB::bind_method(
        godot::D_METHOD("initialize", "map_json"),
        &SimServer::initialize);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_local_input", "move", "aim", "fire", "seq"),
        &SimServer::set_local_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_cast_input", "cast_slot", "confirm", "cancel", "interrupt", "aim_x", "aim_y", "target_id"),
        &SimServer::set_cast_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_move_command", "target_x", "target_y", "issue"),
        &SimServer::set_move_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_stop", "stop"),
        &SimServer::set_stop);
    godot::ClassDB::bind_method(
        godot::D_METHOD("tick", "delta"),
        &SimServer::tick);
    godot::ClassDB::bind_method(
        godot::D_METHOD("pop_snapshot"),
        &SimServer::pop_snapshot);
    godot::ClassDB::bind_method(
        godot::D_METHOD("is_game_over"),
        &SimServer::is_game_over);
}

void SimServer::initialize(const godot::String &p_map_json) {
    godot::UtilityFunctions::print("SimServer::initialize called");
    _world.initialize(p_map_json.utf8().get_data());
}

void SimServer::set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim,
                                 bool fire, int seq) {
    _world.set_local_input(
        sim::Vec2{static_cast<float>(move.x), static_cast<float>(move.y)},
        sim::Vec2{static_cast<float>(aim.x), static_cast<float>(aim.y)},
        fire, seq);
}

void SimServer::set_cast_input(int cast_slot, bool confirm, bool cancel, bool interrupt, float aim_x, float aim_y, int target_id) {
    _world.set_cast_input(cast_slot, confirm, cancel, interrupt, aim_x, aim_y, target_id);
}

void SimServer::set_move_command(float target_x, float target_y, bool issue) {
    _world.set_move_command(target_x, target_y, issue);
}

void SimServer::set_stop(bool stop) {
    _world.set_stop(stop);
}

void SimServer::tick(double delta) {
    _world.tick(delta);
}

bool SimServer::is_game_over() {
    return _world.is_game_over();
}

godot::Ref<godot::RefCounted> SimServer::pop_snapshot() {
    return _world.pop_snapshot();
}
