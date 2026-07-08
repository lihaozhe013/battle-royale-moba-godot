#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "bot_role_rules.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <random>
#include <vector>

namespace sim {

namespace detail {

struct _PickupInfo {
    entt::entity entity;
    float dist_sq;
};
using _PickupVec = std::vector<_PickupInfo>;

inline void _scan_pickups(
    entt::registry &reg, Vec2 bot_pos, PickupType type, _PickupVec &out
) {
    out.clear();
    auto view = reg.view<PickupTag, Position2D>();
    for (auto p : view) {
        if (view.get<PickupTag>(p).Type != type)
            continue;
        Vec2 delta = view.get<Position2D>(p).Value - bot_pos;
        out.push_back({p, vec2_length_sq(delta)});
    }
    if (out.size() > 1) {
        std::sort(out.begin(), out.end(), [](auto &a, auto &b) {
            return a.dist_sq < b.dist_sq;
        });
    }
}

inline entt::entity _pick_top3_random(_PickupVec &vec, std::mt19937 &rng) {
    if (vec.empty())
        return entt::null;
    size_t n = std::min<size_t>(vec.size(), 3);
    std::uniform_int_distribution<size_t> dist(0, n - 1);
    return vec[dist(rng)].entity;
}

// perpendicular vector (left)
inline Vec2 _perp_left(const Vec2 &v) { return Vec2{-v.y, v.x}; }

} // namespace detail

inline void bot_ai_system(
    entt::registry &reg, float dt, float map_half, std::mt19937 &rng
) {
    auto view = reg.view<
        BotTag,
        Position2D,
        FacingAngle,
        BotAIState,
        BotBehaviorState,
        MoveSpeed,
        Health,
        CombatStats,
        Level,
        Experience,
        BotVisionRange>();
    auto target_view = reg.view<Damageable, Position2D, Health>();

    detail::_PickupVec heal_pickups, xp_pickups, small_heal_pickups;

    for (auto e : view) {
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &beh = view.get<BotBehaviorState>(e);
        auto &speed = view.get<MoveSpeed>(e);
        auto &hp = view.get<Health>(e);
        auto &stats = view.get<CombatStats>(e);
        auto &lv = view.get<Level>(e);
        auto &exp = view.get<Experience>(e);
        auto &vision = view.get<BotVisionRange>(e);
        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;

        // ── Dead → Respawn ──
        if (dead) {
            ai.RespawnTimer -= dt;
            if (ai.RespawnTimer <= 0.0f) {
                reg.get<Dead>(e).enabled = false;

                // step 1: scan alive bot role distribution
                int counts[3] = {0, 0, 0};
                auto role_view = reg.view<BotTag, BotRole>(entt::exclude<Dead>);
                for (auto b : role_view) {
                    counts[static_cast<int>(role_view.get<BotRole>(b))]++;
                }

                // step 2: select least dense role
                float density[3] = {
                    counts[0] / (float)GameConfig::FodderWeight,
                    counts[1] / (float)GameConfig::StalkerWeight,
                    counts[2] / (float)GameConfig::BruteWeight,
                };
                BotRole role;
                if (density[0] <= density[1] && density[0] <= density[2])
                    role = BotRole::Fodder;
                else if (density[1] <= density[2])
                    role = BotRole::Stalker;
                else
                    role = BotRole::Brute;

                reg.get_or_emplace<BotRole>(e) = role;

                // step 3: roll level by role
                int new_lv = detail::roll_bot_level_for_role(reg, role, rng);

                // step 4: roll tier by role
                BotTier tier = detail::roll_bot_tier_for_role(role, rng);
                auto mult = detail::tier_mult(tier);

                // step 5: apply stats
                reg.get_or_emplace<BotTier>(e) = tier;
                lv.Value = new_lv;
                int base_hp = GameConfig::BotHp +
                              (new_lv - 1) * GameConfig::BotHpPerLevel;
                hp.Max = static_cast<int>(base_hp * mult.HpMul);
                hp.Cur = hp.Max;
                stats.Atk = (GameConfig::BotBaseAttack +
                             (new_lv - 1) * GameConfig::BotAtkPerLevel) *
                            mult.AtkMul;
                stats.Asp = std::min(
                    (GameConfig::BotBaseAttackSpeed +
                     (new_lv - 1) * GameConfig::BotAspPerLevel) *
                        mult.AspMul,
                    GameConfig::AspMax
                );
                speed.Value = (GameConfig::BotSpeed +
                               (new_lv - 1) * GameConfig::BotSpeedPerLevel) *
                              mult.SpeedMul;
                vision.Value = GameConfig::BotVisionRange * mult.VisionMul;
                exp.Cur = 0;
                exp.Needed = new_lv * GameConfig::XpPerLevelBase;

                // step 6: respawn position + wander reset
                float half = map_half - GameConfig::BotRadius;
                pos.Value = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.MoveTarget = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.RespawnTimer = 0.0f;
                ai.TargetEntity = entt::null;
                ai.WanderTimer =
                    GameConfig::BotWanderIntervalMin +
                    std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) *
                        (GameConfig::BotWanderIntervalMax -
                         GameConfig::BotWanderIntervalMin);
                beh.Current = BotBehaviorState::Goal::Wander;
                beh.PickupTarget = entt::null;
            }
            continue;
        }

        // ── Decision cooldown ──
        beh.DecisionCooldown -= dt;
        bool can_decide = beh.DecisionCooldown <= 0.0f;
        if (can_decide) {
            beh.DecisionCooldown = GameConfig::BotDecisionCooldown;
        }

        // ── Find nearest visible alive target (from bot_targeting, already set
        // ai.TargetEntity) ── bot_targeting_system runs before us, so
        // ai.TargetEntity is already populated. We just read it.

        // ── Scan pickups once per decision cycle ──
        if (can_decide) {
            detail::_scan_pickups(
                reg, pos.Value, PickupType::Heal, heal_pickups
            );
            detail::_scan_pickups(
                reg, pos.Value, PickupType::SmallHeal, small_heal_pickups
            );
            detail::_scan_pickups(reg, pos.Value, PickupType::Xp, xp_pickups);

            // Merge heal pickups: Heal (big) first, then SmallHeal
            heal_pickups.insert(
                heal_pickups.end(),
                small_heal_pickups.begin(),
                small_heal_pickups.end()
            );
            if (heal_pickups.size() > 1) {
                std::sort(
                    heal_pickups.begin(),
                    heal_pickups.end(),
                    [](auto &a, auto &b) { return a.dist_sq < b.dist_sq; }
                );
            }

            // Check if current target is still valid
            bool has_target = ai.TargetEntity != entt::null &&
                              reg.valid(ai.TargetEntity) &&
                              (!reg.all_of<Dead>(ai.TargetEntity) ||
                               !reg.get<Dead>(ai.TargetEntity).enabled);

            // Find nearest alive enemy distance
            float nearest_enemy_dist_sq = std::numeric_limits<float>::max();
            Vec2 nearest_enemy_pos{0, 0};
            for (auto tgt : target_view) {
                if (tgt == e)
                    continue;
                if (reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled)
                    continue;
                Vec2 delta = target_view.get<Position2D>(tgt).Value - pos.Value;
                float d_sq = vec2_length_sq(delta);
                if (d_sq < nearest_enemy_dist_sq) {
                    nearest_enemy_dist_sq = d_sq;
                    nearest_enemy_pos = target_view.get<Position2D>(tgt).Value;
                }
            }
            bool enemy_in_vision =
                nearest_enemy_dist_sq < vision.Value * vision.Value;
            float hp_ratio =
                static_cast<float>(hp.Cur) / static_cast<float>(hp.Max);

            // ── Fix D: Goal 承诺计时器 ──
            beh.GoalCommitTimer -= dt;
            bool can_change_goal = beh.GoalCommitTimer <= 0.0f;

            // ── Goal selection (priority high → low) ──
            BotBehaviorState::Goal new_goal = beh.Current;
            entt::entity new_pickup = entt::null;
            bool goal_changed = false;

            // 紧急条件（永远可以打断承诺）
            bool emergency = hp_ratio < 0.3f && enemy_in_vision;

            if (emergency) {
                new_goal = BotBehaviorState::Goal::Flee;
                goal_changed = true;
            } else if (can_change_goal) {
                if (hp_ratio < 0.6f && !heal_pickups.empty()) {
                    new_goal = BotBehaviorState::Goal::SeekHeal;
                    new_pickup = detail::_pick_top3_random(heal_pickups, rng);
                    goal_changed = true;
                } else if (
                    (!has_target || !enemy_in_vision) && !xp_pickups.empty()
                ) {
                    new_goal = BotBehaviorState::Goal::SeekXp;
                    new_pickup = detail::_pick_top3_random(xp_pickups, rng);
                    goal_changed = true;
                } else if (has_target) {
                    new_goal = BotBehaviorState::Goal::Engage;
                    goal_changed = true;
                } else {
                    new_goal = BotBehaviorState::Goal::Wander;
                    goal_changed = true;
                }
            } // else: 承诺期内保持当前 Goal

            if (goal_changed) {
                beh.Current = new_goal;
                beh.GoalCommitTimer = GameConfig::BotGoalCommitTime;
            }
            beh.PickupTarget = new_pickup;

            // ── Fix A: strafe 方向在决策段决定 ──
            if (beh.Current == BotBehaviorState::Goal::Engage) {
                std::bernoulli_distribution coin(0.5);
                beh.StrafeDir = coin(rng) ? 1 : -1;
            }
        }

        // ── Movement ──
        Vec2 target_pos = ai.MoveTarget; // default: wander target

        switch (beh.Current) {
        case BotBehaviorState::Goal::Flee: {
            // Run away from nearest enemy
            float nearest_dist = std::numeric_limits<float>::max();
            Vec2 flee_from{0, 0};
            for (auto tgt : target_view) {
                if (tgt == e)
                    continue;
                if (reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled)
                    continue;
                Vec2 delta = target_view.get<Position2D>(tgt).Value - pos.Value;
                float d_sq = vec2_length_sq(delta);
                if (d_sq < nearest_dist) {
                    nearest_dist = d_sq;
                    flee_from = target_view.get<Position2D>(tgt).Value;
                }
            }
            if (nearest_dist < std::numeric_limits<float>::max()) {
                Vec2 away_dir = pos.Value - flee_from;
                float len = glm::length(away_dir);
                if (len > 0.01f) {
                    target_pos =
                        pos.Value + (away_dir / len) * GameConfig::BotFleeDist;
                }
            }
            break;
        }
        case BotBehaviorState::Goal::SeekHeal: {
            if (beh.PickupTarget != entt::null && reg.valid(beh.PickupTarget) &&
                reg.all_of<Position2D>(beh.PickupTarget)) {
                target_pos = reg.get<Position2D>(beh.PickupTarget).Value;
            }
            break;
        }
        case BotBehaviorState::Goal::SeekXp: {
            if (beh.PickupTarget != entt::null && reg.valid(beh.PickupTarget) &&
                reg.all_of<Position2D>(beh.PickupTarget)) {
                target_pos = reg.get<Position2D>(beh.PickupTarget).Value;
            } else {
                detail::_scan_pickups(
                    reg, pos.Value, PickupType::Xp, xp_pickups
                );
                if (!xp_pickups.empty()) {
                    target_pos =
                        reg.get<Position2D>(xp_pickups[0].entity).Value;
                }
            }
            break;
        }
        case BotBehaviorState::Goal::Engage: {
            // ── Fix C: Kiting 滞回状态机 ──
            if (ai.TargetEntity != entt::null && reg.valid(ai.TargetEntity) &&
                reg.all_of<Position2D>(ai.TargetEntity)) {
                Vec2 tgt_pos = reg.get<Position2D>(ai.TargetEntity).Value;
                Vec2 to_target = tgt_pos - pos.Value;
                float dist = glm::length(to_target);
                if (dist > 0.001f) {
                    Vec2 dir = to_target / dist;
                    float chase_enter =
                        vision.Value * GameConfig::BotKiteChaseEnter;
                    float chase_exit =
                        vision.Value * GameConfig::BotKiteChaseExit;
                    float retreat_exit =
                        vision.Value * GameConfig::BotKiteRetreatExit;
                    float retreat_enter =
                        vision.Value * GameConfig::BotKiteRetreatEnter;

                    // 滞回状态转移
                    switch (beh.Kite) {
                    case BotBehaviorState::KiteSub::Chase:
                        if (dist < chase_exit)
                            beh.Kite = BotBehaviorState::KiteSub::Strafe;
                        break;
                    case BotBehaviorState::KiteSub::Strafe:
                        if (dist > chase_enter)
                            beh.Kite = BotBehaviorState::KiteSub::Chase;
                        else if (dist < retreat_enter)
                            beh.Kite = BotBehaviorState::KiteSub::Retreat;
                        break;
                    case BotBehaviorState::KiteSub::Retreat:
                        if (dist > retreat_exit)
                            beh.Kite = BotBehaviorState::KiteSub::Strafe;
                        break;
                    }

                    // 执行子状态
                    switch (beh.Kite) {
                    case BotBehaviorState::KiteSub::Chase:
                        target_pos = tgt_pos;
                        break;
                    case BotBehaviorState::KiteSub::Retreat:
                        target_pos =
                            pos.Value - dir * GameConfig::BotKiteStrafeDist;
                        break;
                    case BotBehaviorState::KiteSub::Strafe: {
                        Vec2 strafe = (beh.StrafeDir > 0)
                                          ? detail::_perp_left(dir)
                                          : Vec2{dir.y, -dir.x};
                        target_pos =
                            pos.Value + strafe * GameConfig::BotKiteStrafeDist;
                        break;
                    }
                    }
                }
            }
            break;
        }
        case BotBehaviorState::Goal::Wander:
        default: {
            // Wander: pick new target when close enough or timer expires
            ai.WanderTimer -= dt;
            Vec2 diff = ai.MoveTarget - pos.Value;
            if (ai.WanderTimer <= 0.0f || vec2_length_sq(diff) < 0.25f) {
                float half = map_half - GameConfig::BotRadius;
                ai.MoveTarget = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.WanderTimer =
                    GameConfig::BotWanderIntervalMin +
                    std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) *
                        (GameConfig::BotWanderIntervalMax -
                         GameConfig::BotWanderIntervalMin);
            }
            target_pos = ai.MoveTarget;
            break;
        }
        }

        // ── Root gate (skip movement, can still shoot) ──
        bool rooted = reg.all_of<StatusEffect>(e) &&
                      reg.get<StatusEffect>(e).Type == StatusType::Root &&
                      reg.get<StatusEffect>(e).Timer > 0.0f;

        if (!rooted) {
            // ── Execute movement ──
            Vec2 move_dir = target_pos - pos.Value;
            float move_dist = glm::length(move_dir);
            if (move_dist > 0.01f) {
                Vec2 dir = move_dir / move_dist;
                float step_len = std::min(move_dist, speed.Value * dt);
                pos.Value =
                    vec2_clamp_to_map(pos.Value + dir * step_len, map_half);
                angle.Radians = std::atan2(dir.y, dir.x);
            }
        }

        // ── Wander timer decay (even when not in Wander state) ──
        // So that when we switch back to Wander, timer has progressed.
        if (beh.Current != BotBehaviorState::Goal::Wander) {
            ai.WanderTimer -= dt;
            if (ai.WanderTimer <= 0.0f) {
                float half = map_half - GameConfig::BotRadius;
                ai.MoveTarget = Vec2{
                    std::uniform_real_distribution<float>(-half, half)(rng),
                    std::uniform_real_distribution<float>(-half, half)(rng)
                };
                ai.WanderTimer =
                    GameConfig::BotWanderIntervalMin +
                    std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) *
                        (GameConfig::BotWanderIntervalMax -
                         GameConfig::BotWanderIntervalMin);
            }
        }
    }
}

} // namespace sim
