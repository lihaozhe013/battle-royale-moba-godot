#pragma once

#include "../components.h"
#include <cstdio>
#include <entt/entt.hpp>

#define BOT_INPUT_DEBUG 0

namespace sim {

inline void bot_input_injection_system(entt::registry &reg) {
    auto view =
        reg.view<HeroTag, HeroInputState, BotAIState, BotBehaviorState>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled)
            continue;
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal)
            continue;

        auto &input = view.get<HeroInputState>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &beh = view.get<BotBehaviorState>(e);

        input.MoveTarget = ai.MoveTarget;
        input.MoveIssue = true;
        input.Stop = false;

        if (reg.all_of<BotCastRequest>(e)) {
            auto &rq = reg.get<BotCastRequest>(e);
            if (rq.Valid && rq.TargetSlot >= 0 && rq.TargetSlot < 4) {
                input.SkillSlot = rq.TargetSlot;
                input.SkillConfirm = true;
                input.SkillAim = rq.AimPos;
                input.SkillTargetId = rq.TargetNetworkId;
#if BOT_INPUT_DEBUG
                printf(
                    "[BOT_INJ] slot=%d skill=%d aim=(%.1f,%.1f) "
                    "target_net=%d\n",
                    rq.TargetSlot,
                    reg.get<SkillComponent>(e).Slots[rq.TargetSlot].SkillId,
                    rq.AimPos.x,
                    rq.AimPos.y,
                    rq.TargetNetworkId
                );
#endif
            } else {
                input.SkillSlot = -1;
                input.SkillConfirm = false;
            }
            rq.Valid = false;
            rq.TargetSlot = -1;
        }

        if (input.SkillSlot < 0 && ai.TargetEntity != entt::null &&
            reg.valid(ai.TargetEntity)) {
            int net_id = reg.all_of<NetworkId>(ai.TargetEntity)
                             ? reg.get<NetworkId>(ai.TargetEntity).Value
                             : -1;
            if (net_id > 0) {
                input.AttackTargetId = net_id;
                input.AttackClear = false;
#if BOT_INPUT_DEBUG
                printf(
                    "[BOT_INJ] atk_target=%d goal=%d\n",
                    net_id,
                    (int)beh.Current
                );
#endif
            }
        } else {
            input.AttackClear = true;
            input.AttackTargetId = -1;
        }

        input.SkillUpgradeSlot = -1;
        input.CancelSkill = false;
        input.CancelAttack = false;
        input.AttackGround = false;
    }
}

} // namespace sim
