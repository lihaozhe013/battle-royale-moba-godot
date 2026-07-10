#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include <cstdio>
#include <entt/entt.hpp>

namespace sim {

inline entt::entity resolve_target_by_netid(
    entt::registry &reg, int net_id
) {
    if (net_id < 0) return entt::null;
    auto view = reg.view<NetworkId, Damageable>();
    for (auto e : view) {
        if (view.get<NetworkId>(e).Value == net_id) return e;
    }
    return entt::null;
}

inline entt::entity find_nearest_enemy(
    entt::registry &reg, Vec2 pos, float max_radius, entt::entity exclude
) {
    entt::entity best = entt::null;
    float best_sq = max_radius * max_radius;
    auto view = reg.view<Damageable, Position2D>();
    for (auto e : view) {
        if (e == exclude) continue;
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        Vec2 delta = view.get<Position2D>(e).Value - pos;
        float dist_sq = vec2_length_sq(delta);
        if (dist_sq < best_sq) {
            best_sq = dist_sq;
            best = e;
        }
    }
    return best;
}

inline void player_attack_command_system(entt::registry &reg, float dt) {
    auto view = reg.view<
        PlayerTag, PlayerInputState, Position2D, AttackTarget, CastState>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &at = view.get<AttackTarget>(e);

        // 1. Stun gate — 眩晕中不清目标
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f)
                continue;
        }

        // 2. 清锁信号（先清，避免清掉下面新设的 pending）
        if (input.AttackClear || input.Stop || input.MoveIssue) {
            if (at.Target != entt::null)
                printf("[ATK] clear target (clear=%d stop=%d move=%d)\n", input.AttackClear, input.Stop, input.MoveIssue);
            at.Target = entt::null;
            at.TargetNetworkId = -1;
            at.PendingTargetId = -1;
            at.PendingGround = false;
        }

        // 3. 存储 pending 命令
        if (input.AttackTargetId >= 0) {
            at.PendingTargetId = input.AttackTargetId;
            at.PendingGround = false;
        }
        if (input.AttackGround) {
            at.PendingGround = true;
            at.PendingGroundPos = input.AttackGroundPos;
        }

        // 4. CastState != None → 跳过消费
        auto &cs = view.get<CastState>(e);
        if (cs.State != CastState::Phase::None) {
            printf("[ATK] command skip cast state=%d\n", (int)cs.State);
            continue;
        }

        // 5. 消费 pending
        if (at.PendingTargetId >= 0) {
            printf("[ATK] consume pending target=%d\n", at.PendingTargetId);
            entt::entity tgt = resolve_target_by_netid(reg, at.PendingTargetId);
            if (tgt != entt::null) {
                bool dead = reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled;
                if (!dead) {
                    bool new_target = (tgt != at.Target);
                    at.Target = tgt;
                    at.TargetNetworkId = at.PendingTargetId;
                    // 新目标 → 立即清攻速计时器，保证第一箭立刻射出
                    if (new_target && reg.all_of<CombatStats>(e))
                        reg.get<CombatStats>(e).LastFireTime = -999.0;
                }
            }
            at.PendingTargetId = -1;
        } else if (at.PendingGround) {
            printf("[ATK] consume pending ground\n");
            entt::entity tgt = find_nearest_enemy(
                reg, at.PendingGroundPos, GameConfig::AttackAcquisitionRange, e);
            if (tgt != entt::null) {
                int net_id = reg.all_of<NetworkId>(tgt)
                    ? reg.get<NetworkId>(tgt).Value : -1;
                bool new_target = (tgt != at.Target);
                at.Target = tgt;
                at.TargetNetworkId = net_id;
                printf("[ATK] ground found target=%d\n", net_id);
                // 新目标 → 立即清攻速计时器，保证第一箭立刻射出
                if (new_target && reg.all_of<CombatStats>(e))
                    reg.get<CombatStats>(e).LastFireTime = -999.0;
            }
            at.PendingGround = false;
        }

        // 6. 验证当前 Target
        if (at.Target != entt::null) {
            if (!reg.valid(at.Target) ||
                (reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled)) {
                printf("[ATK] target invalidated\n");
                at.Target = entt::null;
                at.TargetNetworkId = -1;
            }
        }
    }
}

} // namespace sim
