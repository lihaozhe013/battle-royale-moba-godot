#include "game_config.h"
#include "world.h"

namespace sim {

void World::set_skill_command(
    int slot, bool confirm, float ax, float ay, int target_id
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.SkillSlot = slot;
        li.SkillConfirm = confirm;
        li.SkillAim = Vec2{ax, ay};
        li.SkillTargetId = target_id;
    }
}

void World::set_skill_upgrade_command(int slot) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.SkillUpgradeSlot = slot;
    }
}

void World::set_attack_command(
    int target_id, bool ground, float gx, float gy, bool clear
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.AttackTargetId = target_id;
        li.AttackGround = ground;
        li.AttackGroundPos = Vec2{gx, gy};
        li.AttackClear = clear;
    }
}

void World::set_cancel_command(bool skill, bool attack) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.CancelSkill = skill;
        li.CancelAttack = attack;
    }
}

void World::set_move_command(float target_x, float target_y, bool issue) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.MoveTarget = Vec2{target_x, target_y};
        li.MoveIssue = issue;
    }
}

void World::set_stop_command(bool stop) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.Stop = stop;
    }
}

void World::set_local_input(
    const Vec2 &move, const Vec2 &aim, bool fire, int seq
) {}

void World::set_cast_input(
    int cast_slot,
    bool confirm,
    bool cancel,
    bool interrupt,
    float aim_x,
    float aim_y,
    int target_id
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.SkillSlot = cast_slot;
        if (confirm)
            li.SkillConfirm = true;
        if (cancel)
            li.CancelSkill = true;
        if (interrupt)
            li.CancelSkill = true;
        li.SkillAim = Vec2{aim_x, aim_y};
        li.SkillTargetId = target_id;
    }
}

} // namespace sim