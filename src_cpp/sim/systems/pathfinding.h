#pragma once

#include "../components.h"
#include "../game_config.h"
#include "../navigation_pipeline.h"
#include "../skills/skill_registry.h"
#include "timed_modifiers.h"
#include <algorithm>
#include <entt/entt.hpp>
#include <vector>

namespace sim {

namespace detail {

inline void invalidate_path_intent(PathQueryState &state) {
    if (state.HasIntent || state.InFlight || state.Dirty)
        ++state.IntentVersion;
    state.HasIntent = false;
    state.TargetEntity = entt::null;
    state.Dirty = false;
    state.InFlight = false;
    state.InFlightVersion = 0;
    state.InFlightRequestId = 0;
    state.RetryAfterTick = 0;
}

inline bool set_path_intent(
    PathQueryState &state,
    PathQueryChannel channel,
    PathQueryPriority priority,
    PathQueryDestination destination,
    entt::entity target_entity,
    Vec2 target,
    float deadzone_sq
) {
    const bool changed =
        !state.HasIntent || state.Channel != channel ||
        state.Priority != priority || state.Destination != destination ||
        state.TargetEntity != target_entity ||
        vec2_length_sq(target - state.DesiredTarget) >= deadzone_sq;
    if (!changed)
        return false;

    ++state.IntentVersion;
    if (state.HasIntent && state.Channel != channel) {
        // The previous channel remains physically in flight, but its result
        // is now stale and a new channel may submit immediately.
        state.InFlight = false;
        state.InFlightVersion = 0;
        state.InFlightRequestId = 0;
    }
    state.Channel = channel;
    state.Priority = priority;
    state.Destination = destination;
    state.TargetEntity = target_entity;
    state.DesiredTarget = target;
    state.HasIntent = true;
    state.Dirty = true;
    state.RetryAfterTick = 0;
    return true;
}

inline void apply_direct_path(
    MovePath &path, PathQueryState &state, Vec2 target
) {
    path.Waypoints = {target};
    path.CurrentIndex = 0;
    path.FinalTarget = target;
    path.Following = true;
    state.Dirty = false;
    state.InFlight = false;
    state.InFlightVersion = 0;
    state.InFlightRequestId = 0;
}

struct PendingRequest {
    entt::entity entity = entt::null;
    NavigationRequest request;
};

} // namespace detail

inline void pathfinding_system(
    entt::registry &reg,
    NavigationPipeline &pipeline,
    int current_tick,
    int max_submissions,
    int failure_retry_ticks
) {
    auto nav = pipeline.nav_grid();
    std::vector<NavigationResult> completed;
    pipeline.poll_completed(current_tick, completed);
    std::sort(
        completed.begin(),
        completed.end(),
        [](const NavigationResult &a, const NavigationResult &b) {
            return a.Request.RequestId < b.Request.RequestId;
        }
    );

    // Apply completed results before gathering new intents. This makes a
    // stop, death, cast transition, or target change invalidate an old result
    // before it can affect movement.
    for (auto &result : completed) {
        const auto entity = result.Request.Requester;
        if (!reg.valid(entity) || !reg.all_of<PathQueryState>(entity)) {
            pipeline.note_stale_result();
            continue;
        }

        auto &state = reg.get<PathQueryState>(entity);
        // A target update keeps the previous job in flight to enforce one
        // request per (entity, channel). When that old result arrives, clear
        // the slot by request id even though its intent version is stale so
        // the newest target can be submitted on the next pass.
        const bool owns_request =
            state.InFlight &&
            state.InFlightRequestId == result.Request.RequestId;
        const bool owns_result =
            owns_request && state.InFlightVersion == result.Request.IntentVersion;
        if (owns_request) {
            state.InFlight = false;
            state.InFlightVersion = 0;
            state.InFlightRequestId = 0;
        }

        const bool target_current =
            result.Request.TargetEntity == entt::null ||
            (state.TargetEntity == result.Request.TargetEntity &&
             reg.valid(result.Request.TargetEntity) &&
             reg.all_of<Position2D>(result.Request.TargetEntity) &&
             !(reg.all_of<Dead>(result.Request.TargetEntity) &&
               reg.get<Dead>(result.Request.TargetEntity).enabled));
        const bool requester_current =
            reg.all_of<Position2D>(entity) &&
            !(reg.all_of<Dead>(entity) && reg.get<Dead>(entity).enabled);
        bool goal_current = true;
        if (result.Request.TargetEntity != entt::null &&
            reg.valid(result.Request.TargetEntity) &&
            reg.all_of<Position2D>(result.Request.TargetEntity)) {
            const float deadzone_sq =
                result.Request.Channel == PathQueryChannel::Skill
                    ? stats(reg).SkillChaseRepathDeadzoneSq
                    : stats(reg).AttackChaseRepathDeadzoneSq;
            goal_current =
                vec2_length_sq(
                    reg.get<Position2D>(result.Request.TargetEntity).Value -
                    result.Request.Goal
                ) < deadzone_sq;
        } else if (
            result.Request.Channel == PathQueryChannel::Skill &&
            result.Request.Destination == PathQueryDestination::Point &&
            reg.all_of<CastState>(entity)) {
            goal_current =
                vec2_length_sq(
                    reg.get<CastState>(entity).AimPos - result.Request.Goal
                ) < stats(reg).SkillChaseRepathDeadzoneSq;
        } else if (
            result.Request.Channel == PathQueryChannel::Movement &&
            result.Request.Destination == PathQueryDestination::Ground &&
            reg.all_of<PlayerInputState>(entity)) {
            const auto &input = reg.get<PlayerInputState>(entity);
            if (input.MoveIssue) {
                goal_current =
                    vec2_length_sq(input.MoveTarget - result.Request.Goal) <
                    stats(reg).RepathTargetDeadzoneSq;
            }
        }
        bool action_current = true;
        if (reg.all_of<CastState>(entity)) {
            const auto cast_phase = reg.get<CastState>(entity).State;
            if (state.Channel == PathQueryChannel::Skill)
                action_current = cast_phase == CastState::Phase::Chasing;
            else if (cast_phase == CastState::Phase::Chasing)
                action_current = false;
            else if (state.Destination == PathQueryDestination::Entity)
                action_current = cast_phase == CastState::Phase::None;
            else
                action_current =
                    cast_phase != CastState::Phase::Casting &&
                    cast_phase != CastState::Phase::Channeling &&
                    cast_phase != CastState::Phase::Dashing;
            if (state.Channel == PathQueryChannel::Skill &&
                state.Destination == PathQueryDestination::Entity) {
                action_current =
                    action_current &&
                    reg.get<CastState>(entity).TargetEntity ==
                        result.Request.TargetEntity;
            }
        }
        if (state.Channel == PathQueryChannel::Movement &&
            state.Destination == PathQueryDestination::Entity) {
            action_current =
                action_current && reg.all_of<AttackTarget>(entity) &&
                reg.get<AttackTarget>(entity).Target ==
                    result.Request.TargetEntity;
        }
        if (state.Destination == PathQueryDestination::Ground &&
            reg.all_of<PlayerInputState>(entity)) {
            action_current = action_current &&
                             !reg.get<PlayerInputState>(entity).Stop;
        }
        const bool current =
            owns_result && state.HasIntent &&
            state.IntentVersion == result.Request.IntentVersion &&
            state.Channel == result.Request.Channel &&
            state.Destination == result.Request.Destination &&
            state.TargetEntity == result.Request.TargetEntity &&
            requester_current && target_current && goal_current &&
            action_current &&
            (!nav || result.Request.NavRevision == nav->Revision);
        if (!current) {
            pipeline.note_stale_result();
            if (owns_request && !goal_current &&
                state.IntentVersion == result.Request.IntentVersion)
                pipeline.note_merged_request();
            if (state.HasIntent && !state.InFlight)
                state.Dirty = true;
            continue;
        }

        auto &path = reg.get_or_emplace<MovePath>(entity);
        auto mark_attack_chasing = [&](bool chasing) {
            if (reg.all_of<AttackTarget>(entity))
                reg.get<AttackTarget>(entity).Chasing = chasing;
        };

        if (result.Status == NavigationResultStatus::Success) {
            if (result.Waypoints.empty()) {
                path.Waypoints.clear();
                path.CurrentIndex = 0;
                path.Following = false;
                mark_attack_chasing(false);
                state.Dirty = false;
                state.RetryAfterTick = 0;
                continue;
            }

            bool first_segment_clear = true;
            if (nav && reg.all_of<Position2D>(entity)) {
                first_segment_clear = nav->line_clear(
                    reg.get<Position2D>(entity).Value,
                    result.Waypoints.front()
                );
            }
            if (!first_segment_clear) {
                pipeline.note_stale_result();
                path.Following = false;
                state.Dirty = true;
                state.RetryAfterTick = current_tick;
                mark_attack_chasing(false);
                continue;
            }

            path.Waypoints = std::move(result.Waypoints);
            path.CurrentIndex = 0;
            path.FinalTarget = state.DesiredTarget;
            path.Following = true;
            state.Dirty = false;
            state.RetryAfterTick = 0;
            mark_attack_chasing(
                state.Channel == PathQueryChannel::Movement &&
                reg.all_of<AttackTarget>(entity)
            );
        } else {
            path.Waypoints.clear();
            path.CurrentIndex = 0;
            path.Following = false;
            state.Dirty = false;
            state.RetryAfterTick = current_tick + failure_retry_ticks;
            mark_attack_chasing(false);
        }
    }

    auto view = reg.view<
        PlayerTag,
        Position2D,
        PlayerInputState,
        CastState,
        MovePath>();
    std::vector<detail::PendingRequest> pending;

    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        const bool is_bot = reg.all_of<BotTag>(e);
        if (!tag.IsLocal && !is_bot)
            continue;

        auto &cs = view.get<CastState>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);
        auto &state = reg.get_or_emplace<PathQueryState>(e);

        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) {
            detail::invalidate_path_intent(state);
            path.Following = false;
            continue;
        }

        if (input.Stop) {
            detail::invalidate_path_intent(state);
            path.Following = false;
            if (reg.all_of<AttackTarget>(e))
                reg.get<AttackTarget>(e).Chasing = false;
            continue;
        }

        if (cs.State == CastState::Phase::Chasing) {
            const ISkill *skill = SkillRegistry::instance().get(cs.SkillId);
            if (!skill) {
                detail::invalidate_path_intent(state);
                path.Following = false;
                continue;
            }

            Vec2 target;
            bool valid_target = true;
            if (skill->target_mode() == SkillTargetMode::Unit) {
                valid_target =
                    reg.valid(cs.TargetEntity) &&
                    reg.all_of<Position2D>(cs.TargetEntity) &&
                    !(reg.all_of<Dead>(cs.TargetEntity) &&
                      reg.get<Dead>(cs.TargetEntity).enabled);
                if (valid_target)
                    target = reg.get<Position2D>(cs.TargetEntity).Value;
            } else if (skill->target_mode() == SkillTargetMode::Point) {
                target = cs.AimPos;
            } else {
                valid_target = false;
            }

            if (!valid_target) {
                detail::invalidate_path_intent(state);
                path.Following = false;
                continue;
            }

            const bool was_in_flight = state.InFlight;
            const bool changed = detail::set_path_intent(
                state,
                PathQueryChannel::Skill,
                PathQueryPriority::High,
                skill->target_mode() == SkillTargetMode::Unit
                    ? PathQueryDestination::Entity
                    : PathQueryDestination::Point,
                skill->target_mode() == SkillTargetMode::Unit
                    ? cs.TargetEntity
                    : entt::null,
                target,
                stats(reg).SkillChaseRepathDeadzoneSq
            );
            if (changed && was_in_flight)
                pipeline.note_merged_request();
            if (has_timed_modifier(
                    reg, e, TimedModifierType::IgnoreTerrain
                )) {
                if (state.InFlight)
                    ++state.IntentVersion;
                detail::apply_direct_path(path, state, target);
            } else if (!path.Following && !state.InFlight &&
                       current_tick >= state.RetryAfterTick) {
                // A completed/invalidated path must be refreshed when the
                // chase is still outside the skill's cast range.
                state.Dirty = true;
            }
            continue;
        }

        if (cs.State == CastState::Phase::Casting ||
            cs.State == CastState::Phase::Channeling ||
            cs.State == CastState::Phase::Dashing) {
            detail::invalidate_path_intent(state);
            path.Following = false;
            continue;
        }

        // A cancelled or completed chase can leave the cast state in None.
        // Drop the old skill intent so a later input cannot resume it.
        if (state.HasIntent && state.Channel == PathQueryChannel::Skill) {
            detail::invalidate_path_intent(state);
            path.Following = false;
        }

        bool handled_attack = false;
        if (cs.State == CastState::Phase::None &&
            reg.all_of<AttackTarget>(e)) {
            auto &attack = reg.get<AttackTarget>(e);
            if (attack.Target != entt::null && reg.valid(attack.Target) &&
                reg.all_of<Position2D>(attack.Target)) {
                const bool dead =
                    reg.all_of<Dead>(attack.Target) &&
                    reg.get<Dead>(attack.Target).enabled;
                if (!dead) {
                    const Vec2 target =
                        reg.get<Position2D>(attack.Target).Value;
                    const float range = reg.all_of<AttackProfile>(e)
                                            ? reg.get<AttackProfile>(e).Range
                                            : 0.0f;
                    const float distance_sq = vec2_length_sq(target - pos.Value);
                    if (distance_sq > range * range) {
                        const bool was_in_flight = state.InFlight;
                        const bool changed = detail::set_path_intent(
                            state,
                            PathQueryChannel::Movement,
                            PathQueryPriority::Normal,
                            PathQueryDestination::Entity,
                            attack.Target,
                            target,
                            stats(reg).AttackChaseRepathDeadzoneSq
                        );
                        if (changed && was_in_flight)
                            pipeline.note_merged_request();
                        if (has_timed_modifier(
                                reg, e, TimedModifierType::IgnoreTerrain
                            )) {
                            if (state.InFlight)
                                ++state.IntentVersion;
                            detail::apply_direct_path(path, state, target);
                        } else if (!path.Following && !state.InFlight &&
                                   current_tick >= state.RetryAfterTick) {
                            state.Dirty = true;
                        }
                        attack.Chasing = path.Following;
                        handled_attack = true;
                    } else {
                        detail::invalidate_path_intent(state);
                        path.Following = false;
                        attack.Chasing = false;
                        handled_attack = true;
                    }
                } else if (
                    attack.Chasing || state.TargetEntity != entt::null
                ) {
                    detail::invalidate_path_intent(state);
                    path.Following = false;
                    attack.Chasing = false;
                }
            } else if (
                attack.Chasing || state.TargetEntity != entt::null
            ) {
                // The command system can invalidate a target before this
                // phase; do not keep following its old path.
                detail::invalidate_path_intent(state);
                path.Following = false;
                attack.Chasing = false;
            }
        }
        if (handled_attack)
            continue;

        if (input.MoveIssue) {
            const bool was_in_flight = state.InFlight;
            const bool changed = detail::set_path_intent(
                state,
                PathQueryChannel::Movement,
                tag.IsLocal ? PathQueryPriority::High : PathQueryPriority::Low,
                PathQueryDestination::Ground,
                entt::null,
                input.MoveTarget,
                stats(reg).RepathTargetDeadzoneSq
            );
            if (changed && was_in_flight)
                pipeline.note_merged_request();
            if (has_timed_modifier(reg, e, TimedModifierType::IgnoreTerrain)) {
                if (state.InFlight)
                    ++state.IntentVersion;
                detail::apply_direct_path(path, state, input.MoveTarget);
            } else if (!path.Following && !state.InFlight &&
                       current_tick >= state.RetryAfterTick) {
                state.Dirty = true;
            }
        }
    }

    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        const bool is_bot = reg.all_of<BotTag>(e);
        if (!tag.IsLocal && !is_bot)
            continue;
        if (!reg.all_of<PathQueryState>(e) || !reg.all_of<Position2D>(e))
            continue;

        auto &state = reg.get<PathQueryState>(e);
        if (!state.HasIntent || !state.Dirty || state.InFlight ||
            current_tick < state.RetryAfterTick)
            continue;
        if (has_timed_modifier(reg, e, TimedModifierType::IgnoreTerrain))
            continue;

        NavigationRequest request;
        request.Requester = e;
        request.Channel = state.Channel;
        request.Priority = state.Priority;
        request.Destination = state.Destination;
        request.TargetEntity = state.TargetEntity;
        request.IntentVersion = state.IntentVersion;
        request.Start = reg.get<Position2D>(e).Value;
        request.Goal = state.DesiredTarget;
        request.NavRevision = nav ? nav->Revision : 0;
        request.SubmitTick = current_tick;
        pending.push_back({e, request});
    }

    std::sort(
        pending.begin(),
        pending.end(),
        [&reg](const detail::PendingRequest &a, const detail::PendingRequest &b) {
            if (a.request.Priority != b.request.Priority)
                return static_cast<int>(a.request.Priority) <
                       static_cast<int>(b.request.Priority);
            const int a_id = reg.all_of<NetworkId>(a.entity)
                                 ? reg.get<NetworkId>(a.entity).Value
                                 : static_cast<int>(entt::to_entity(a.entity));
            const int b_id = reg.all_of<NetworkId>(b.entity)
                                 ? reg.get<NetworkId>(b.entity).Value
                                 : static_cast<int>(entt::to_entity(b.entity));
            if (a_id != b_id)
                return a_id < b_id;
            return entt::to_integral(a.entity) < entt::to_integral(b.entity);
        }
    );

    const int budget = std::max(0, max_submissions);
    int submitted = 0;
    for (auto &item : pending) {
        if (submitted >= budget)
            break;
        auto &state = reg.get<PathQueryState>(item.entity);
        uint64_t request_id = 0;
        if (!pipeline.submit(item.request, request_id))
            continue;
        state.InFlight = true;
        state.InFlightVersion = item.request.IntentVersion;
        state.InFlightRequestId = request_id;
        ++submitted;
    }
}

} // namespace sim
