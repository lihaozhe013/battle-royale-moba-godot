#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>
#include <random>
#include <vector>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"

namespace sim {

namespace detail {

struct _PickupInfo {
    entt::entity entity;
    float dist_sq;
};
using _PickupVec = std::vector<_PickupInfo>;

inline void _scan_pickups(entt::registry &reg, Vec2 bot_pos, PickupType type, _PickupVec &out) {
    out.clear();
    auto view = reg.view<PickupTag, Position2D>();
    for (auto p : view) {
        if (view.get<PickupTag>(p).Type != type) continue;
        Vec2 delta = view.get<Position2D>(p).Value - bot_pos;
        out.push_back({p, vec2_length_sq(delta)});
    }
    if (out.size() > 1) {
        std::sort(out.begin(), out.end(),
            [](auto &a, auto &b) { return a.dist_sq < b.dist_sq; });
    }
}

inline entt::entity _pick_top3_random(_PickupVec &vec, std::mt19937 &rng) {
    if (vec.empty()) return entt::null;
    size_t n = std::min<size_t>(vec.size(), 3);
    std::uniform_int_distribution<size_t> dist(0, n - 1);
    return vec[dist(rng)].entity;
}

// perpendicular vector (left)
inline Vec2 _perp_left(const Vec2 &v) {
    return Vec2{-v.y, v.x};
}

} // namespace detail

inline void bot_ai_system(entt::registry &reg, float dt, float map_half, std::mt19937 &rng) {
    auto view = reg.view<BotTag, Position2D, FacingAngle, BotAIState, BotBehaviorState,
                          MoveSpeed, Health, CombatStats, Level, Experience, BotVisionRange>();
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

                int new_lv = std::uniform_int_distribution<int>(1, GameConfig::MaxBotLevel)(rng);
                float r = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
                BotTier tier;
                float hp_mul, atk_mul, asp_mul, speed_mul, vision_mul;
                if (r < GameConfig::BossRoll) {
                    tier = BotTier::Boss;
                    hp_mul = GameConfig::BossHpMul; atk_mul = GameConfig::BossAtkMul;
                    asp_mul = GameConfig::BossAspMul; speed_mul = GameConfig::BossSpeedMul;
                    vision_mul = GameConfig::BossVisionMul;
                } else if (r < GameConfig::EliteRoll) {
                    tier = BotTier::Elite;
                    hp_mul = GameConfig::EliteHpMul; atk_mul = GameConfig::EliteAtkMul;
                    asp_mul = GameConfig::EliteAspMul; speed_mul = GameConfig::EliteSpeedMul;
                    vision_mul = GameConfig::EliteVisionMul;
                } else {
                    tier = BotTier::Normal;
                    hp_mul = GameConfig::NormalHpMul; atk_mul = GameConfig::NormalAtkMul;
                    asp_mul = GameConfig::NormalAspMul; speed_mul = GameConfig::NormalSpeedMul;
                    vision_mul = GameConfig::NormalVisionMul;
                }

                lv.Value = new_lv;
                int base_hp = GameConfig::BotHp + (new_lv - 1) * GameConfig::BotHpPerLevel;
                hp.Max = static_cast<int>(base_hp * hp_mul);
                hp.Cur = hp.Max;
                stats.Atk = (GameConfig::BotBaseAttack + (new_lv - 1) * GameConfig::BotAtkPerLevel) * atk_mul;
                stats.Asp = std::min(
                    (GameConfig::BotBaseAttackSpeed + (new_lv - 1) * GameConfig::BotAspPerLevel) * asp_mul,
                    GameConfig::AspMax);
                speed.Value = (GameConfig::BotSpeed + (new_lv - 1) * GameConfig::BotSpeedPerLevel) * speed_mul;
                vision.Value = GameConfig::BotVisionRange * vision_mul;
                exp.Cur = 0;
                exp.Needed = new_lv * GameConfig::XpPerLevelBase;

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
                ai.WanderTimer = GameConfig::BotWanderIntervalMin
                    + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)
                    * (GameConfig::BotWanderIntervalMax - GameConfig::BotWanderIntervalMin);
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

        // ── Find nearest visible alive target (from bot_targeting, already set ai.TargetEntity) ──
        // bot_targeting_system runs before us, so ai.TargetEntity is already populated.
        // We just read it.

        // ── Scan pickups once per decision cycle ──
        if (can_decide) {
            detail::_scan_pickups(reg, pos.Value, PickupType::Heal, heal_pickups);
            detail::_scan_pickups(reg, pos.Value, PickupType::SmallHeal, small_heal_pickups);
            detail::_scan_pickups(reg, pos.Value, PickupType::Xp, xp_pickups);

            // Merge heal pickups: Heal (big) first, then SmallHeal
            heal_pickups.insert(heal_pickups.end(), small_heal_pickups.begin(), small_heal_pickups.end());
            if (heal_pickups.size() > 1) {
                std::sort(heal_pickups.begin(), heal_pickups.end(),
                    [](auto &a, auto &b) { return a.dist_sq < b.dist_sq; });
            }

            // Check if current target is still valid
            bool has_target = ai.TargetEntity != entt::null
                && reg.valid(ai.TargetEntity)
                && (!reg.all_of<Dead>(ai.TargetEntity) || !reg.get<Dead>(ai.TargetEntity).enabled);

            // Find nearest alive enemy distance
            float nearest_enemy_dist_sq = std::numeric_limits<float>::max();
            Vec2 nearest_enemy_pos{0, 0};
            for (auto tgt : target_view) {
                if (tgt == e) continue;
                if (reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled) continue;
                Vec2 delta = target_view.get<Position2D>(tgt).Value - pos.Value;
                float d_sq = vec2_length_sq(delta);
                if (d_sq < nearest_enemy_dist_sq) {
                    nearest_enemy_dist_sq = d_sq;
                    nearest_enemy_pos = target_view.get<Position2D>(tgt).Value;
                }
            }
            bool enemy_in_vision = nearest_enemy_dist_sq < vision.Value * vision.Value;
            float hp_ratio = static_cast<float>(hp.Cur) / static_cast<float>(hp.Max);

            // ── Goal selection (priority high → low) ──
            BotBehaviorState::Goal new_goal;
            entt::entity new_pickup = entt::null;

            if (hp_ratio < 0.3f && enemy_in_vision) {
                // PRIORITY 1: Flee
                new_goal = BotBehaviorState::Goal::Flee;
            } else if (hp_ratio < 0.6f && !heal_pickups.empty()) {
                // PRIORITY 2: SeekHeal
                new_goal = BotBehaviorState::Goal::SeekHeal;
                new_pickup = detail::_pick_top3_random(heal_pickups, rng);
            } else if ((!has_target || !enemy_in_vision) && !xp_pickups.empty()) {
                // PRIORITY 3: SeekXp (safe to farm: no target, or target is far away)
                new_goal = BotBehaviorState::Goal::SeekXp;
                new_pickup = detail::_pick_top3_random(xp_pickups, rng);
            } else if (has_target) {
                // PRIORITY 4: Engage (kiting) — even if enemy is far, push toward them
                new_goal = BotBehaviorState::Goal::Engage;
            } else {
                // PRIORITY 5: Wander — nothing to do
                new_goal = BotBehaviorState::Goal::Wander;
            }

            beh.Current = new_goal;
            beh.PickupTarget = new_pickup;
        }

        // ── Movement ──
        Vec2 target_pos = ai.MoveTarget; // default: wander target
        bool stop_and_shoot = false;

        switch (beh.Current) {
            case BotBehaviorState::Goal::Flee: {
                // Run away from nearest enemy
                float nearest_dist = std::numeric_limits<float>::max();
                Vec2 flee_from{0, 0};
                for (auto tgt : target_view) {
                    if (tgt == e) continue;
                    if (reg.all_of<Dead>(tgt) && reg.get<Dead>(tgt).enabled) continue;
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
                        target_pos = pos.Value + (away_dir / len) * GameConfig::BotFleeDist;
                    }
                }
                break;
            }
            case BotBehaviorState::Goal::SeekHeal: {
                if (beh.PickupTarget != entt::null && reg.valid(beh.PickupTarget)
                    && reg.all_of<Position2D>(beh.PickupTarget)) {
                    target_pos = reg.get<Position2D>(beh.PickupTarget).Value;
                }
                break;
            }
            case BotBehaviorState::Goal::SeekXp: {
                if (beh.PickupTarget != entt::null && reg.valid(beh.PickupTarget)
                    && reg.all_of<Position2D>(beh.PickupTarget)) {
                    target_pos = reg.get<Position2D>(beh.PickupTarget).Value;
                } else {
                    // Fallback: scan for any XP pickup
                    detail::_scan_pickups(reg, pos.Value, PickupType::Xp, xp_pickups);
                    if (!xp_pickups.empty()) {
                        target_pos = reg.get<Position2D>(xp_pickups[0].entity).Value;
                    }
                }
                break;
            }
            case BotBehaviorState::Goal::Engage: {
                if (ai.TargetEntity != entt::null && reg.valid(ai.TargetEntity)
                    && reg.all_of<Position2D>(ai.TargetEntity)) {
                    Vec2 tgt_pos = reg.get<Position2D>(ai.TargetEntity).Value;
                    Vec2 to_target = tgt_pos - pos.Value;
                    float dist = glm::length(to_target);
                    if (dist > 0.001f) {
                        Vec2 dir = to_target / dist;
                        if (dist > vision.Value * GameConfig::BotEngageRangeHigh) {
                            // Chase
                            target_pos = tgt_pos;
                        } else if (dist < vision.Value * GameConfig::BotEngageRangeLow) {
                            // Retreat
                            target_pos = pos.Value - dir * GameConfig::BotKiteStrafeDist;
                        } else {
                            // Strafe (move perpendicular)
                            std::bernoulli_distribution coin(0.5);
                            Vec2 strafe = coin(rng)
                                ? detail::_perp_left(dir)
                                : Vec2{dir.y, -dir.x};
                            target_pos = pos.Value + strafe * GameConfig::BotKiteStrafeDist;
                            stop_and_shoot = true; // can shoot while strafing
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
                    ai.WanderTimer = GameConfig::BotWanderIntervalMin
                        + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)
                        * (GameConfig::BotWanderIntervalMax - GameConfig::BotWanderIntervalMin);
                }
                target_pos = ai.MoveTarget;
                break;
            }
        }

        // ── Execute movement ──
        Vec2 move_dir = target_pos - pos.Value;
        float move_dist = glm::length(move_dir);
        if (move_dist > 0.01f) {
            Vec2 dir = move_dir / move_dist;
            float step_len = std::min(move_dist, speed.Value * dt);
            pos.Value = vec2_clamp_to_map(pos.Value + dir * step_len, map_half);
            angle.Radians = std::atan2(dir.y, dir.x);
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
                ai.WanderTimer = GameConfig::BotWanderIntervalMin
                    + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)
                    * (GameConfig::BotWanderIntervalMax - GameConfig::BotWanderIntervalMin);
            }
        }
    }
}

} // namespace sim
