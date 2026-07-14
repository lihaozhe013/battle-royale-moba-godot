#include "sim_server.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

SimServer::SimServer() {}
SimServer::~SimServer() {}

void SimServer::_bind_methods() {
    // ── 核心方法 ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("initialize", "map_json"),
        &SimServer::initialize);

    // ── v2 新命令 API ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_skill_command", "slot", "confirm", "aim_x", "aim_y", "target_id"),
        &SimServer::set_skill_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_skill_upgrade_command", "slot"),
        &SimServer::set_skill_upgrade_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_attack_command_full", "target_id", "ground", "gx", "gy", "clear"),
        &SimServer::set_attack_command_full);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_cancel_command", "skill", "attack"),
        &SimServer::set_cancel_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_move_command", "target_x", "target_y", "issue"),
        &SimServer::set_move_command);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_stop_command"),
        &SimServer::set_stop_command);

    // ── v1 旧 API（deprecated，临时保留兼容） ──
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_local_input", "move", "aim", "fire", "seq"),
        &SimServer::set_local_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_cast_input", "cast_slot", "confirm", "cancel", "interrupt", "aim_x", "aim_y", "target_id"),
        &SimServer::set_cast_input);
    godot::ClassDB::bind_method(
        godot::D_METHOD("set_attack_command", "target_id"),
        &SimServer::set_attack_command);

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

// ── v2 新命令 API ──
void SimServer::set_skill_command(int slot, bool confirm, float aim_x, float aim_y, int target_id) {
    _world.set_skill_command(slot, confirm, aim_x, aim_y, target_id);
}

void SimServer::set_skill_upgrade_command(int slot) {
    _world.set_skill_upgrade_command(slot);
}

void SimServer::set_attack_command_full(int target_id, bool ground, float gx, float gy, bool clear) {
    _world.set_attack_command(target_id, ground, gx, gy, clear);
}

void SimServer::set_cancel_command(bool skill, bool attack) {
    _world.set_cancel_command(skill, attack);
}

void SimServer::set_move_command(float target_x, float target_y, bool issue) {
    _world.set_move_command(target_x, target_y, issue);
}

void SimServer::set_stop_command() {
    _world.set_stop_command();
}

// ── v1 旧 API（deprecated） ──
void SimServer::set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim,
                                 bool fire, int seq) {
    _world.set_local_input(
        sim::Vec2{static_cast<float>(move.x), static_cast<float>(move.y)},
        sim::Vec2{static_cast<float>(aim.x), static_cast<float>(aim.y)},
        fire, seq);
}

void SimServer::set_cast_input(int cast_slot, bool confirm, bool cancel,
                                bool interrupt, float aim_x, float aim_y, int target_id) {
    _world.set_cast_input(cast_slot, confirm, cancel, interrupt, aim_x, aim_y, target_id);
}

void SimServer::set_attack_command(int target_id) {
    _world.set_attack_command(target_id, false, 0.0f, 0.0f, false);
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
