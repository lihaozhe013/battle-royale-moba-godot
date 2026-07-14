#pragma once

#include "../components.h"
#include <entt/entt.hpp>

namespace sim {

inline void local_input_injection_system(
    entt::registry &reg, entt::entity local_input_entity
) {
    auto &input = reg.get<LocalInputSingleton>(local_input_entity);
    auto view = reg.view<PlayerInputState, PlayerTag>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal)
            continue;
        auto &state = view.get<PlayerInputState>(e);
        // 移动
        state.MoveTarget = input.MoveTarget;
        state.MoveIssue = input.MoveIssue;
        state.Stop = input.Stop;
        // 技能
        state.SkillSlot = input.SkillSlot;
        state.SkillConfirm = input.SkillConfirm;
        state.SkillAim = input.SkillAim;
        state.SkillTargetId = input.SkillTargetId;
        state.SkillUpgradeSlot = input.SkillUpgradeSlot;
        // 取消
        state.CancelSkill = input.CancelSkill;
        state.CancelAttack = input.CancelAttack;
        // 普攻
        state.AttackTargetId = input.AttackTargetId;
        state.AttackGround = input.AttackGround;
        state.AttackGroundPos = input.AttackGroundPos;
        state.AttackClear = input.AttackClear;
        // 序号
        state.Seq = input.Seq;
    }
}

} // namespace sim
