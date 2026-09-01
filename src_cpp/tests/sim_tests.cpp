#include "../sim/components.h"
#include "../sim/game_config.h"
#include "../sim/heroes/hero_registry.h"
#include "../sim/job_system.h"
#include "../sim/nav_grid.h"
#include "../sim/navigation_pipeline.h"
#include "../sim/skills/skill_registry.h"
#include "../sim/stats_config.h"
#include "../sim/systems/attack_fire.h"
#include "../sim/systems/pathfinding.h"
#include "../sim/systems/timed_modifiers.h"
#include <atomic>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string load_fixture() {
    for (const char *path : {"../../data/stats.yaml", "data/stats.yaml"}) {
        std::ifstream input(path);
        if (input)
            return std::string(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>()
            );
    }
    return {};
}

bool replace_once(std::string &text, const std::string &from, const std::string &to) {
    const auto offset = text.find(from);
    if (offset == std::string::npos)
        return false;
    text.replace(offset, from.size(), to);
    return true;
}

void assert_paths_equal(
    const std::vector<sim::Vec2> &a, const std::vector<sim::Vec2> &b
) {
    assert(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        assert(std::abs(a[i].x - b[i].x) < 0.0001f);
        assert(std::abs(a[i].y - b[i].y) < 0.0001f);
    }
}

void test_job_system_and_navigation() {
    sim::JobSystem jobs(2, 64);
    std::atomic<int> completed{0};
    for (int i = 0; i < 32; ++i) {
        const auto priority = i % 3 == 0
                                  ? sim::JobPriority::High
                                  : (i % 3 == 1 ? sim::JobPriority::Normal
                                                : sim::JobPriority::Low);
        assert(jobs.enqueue(priority, [&completed](size_t) {
            completed.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    jobs.wait_idle();
    assert(completed.load(std::memory_order_relaxed) == 32);
    assert(jobs.queued_jobs() == 0);
    assert(jobs.active_jobs() == 0);
    jobs.reconfigure(1);
    assert(jobs.worker_count() == 1);
    assert(jobs.enqueue(sim::JobPriority::High, [&completed](size_t) {
        completed.fetch_add(1, std::memory_order_relaxed);
    }));
    jobs.wait_idle();
    assert(completed.load(std::memory_order_relaxed) == 33);

    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    sim::JobSystem capped_jobs(1, 2);
    assert(capped_jobs.enqueue(sim::JobPriority::High, [&](size_t) {
        started.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
    }));
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
    assert(capped_jobs.enqueue(sim::JobPriority::Low, [](size_t) {}));
    assert(capped_jobs.enqueue(sim::JobPriority::Low, [](size_t) {}));
    assert(!capped_jobs.enqueue(sim::JobPriority::Low, [](size_t) {}));
    release.store(true, std::memory_order_release);
    capped_jobs.wait_idle();

    sim::NavGrid grid;
    std::vector<sim::WallBounds> walls = {
        {sim::Vec2{-1.0f, -8.0f}, sim::Vec2{1.0f, -2.0f}},
        {sim::Vec2{-1.0f, 2.0f}, sim::Vec2{1.0f, 8.0f}},
    };
    grid.build(10.0f, walls, 0.5f, 0.5f);

    assert(grid.find_path({-8.0f, -8.0f}, {-8.0f, -8.0f}).size() == 1);
    assert(grid.find_path({-11.0f, 0.0f}, {8.0f, 8.0f}).empty());
    assert(!grid.find_path({0.0f, -5.0f}, {8.0f, -5.0f}).empty());

    sim::NavGrid unreachable_grid;
    unreachable_grid.build(
        10.0f,
        {{sim::Vec2{-1.0f, -9.0f}, sim::Vec2{1.0f, 9.0f}}},
        0.5f,
        0.5f
    );
    assert(
        unreachable_grid.find_path({-8.0f, 0.0f}, {8.0f, 0.0f}).empty()
    );

    const std::vector<std::pair<sim::Vec2, sim::Vec2>> queries = {
        {{-8.0f, -8.0f}, {8.0f, 8.0f}},
        {{-8.0f, 8.0f}, {8.0f, -8.0f}},
        {{-7.0f, 0.0f}, {7.0f, 0.0f}},
        {{-8.0f, -7.0f}, {7.0f, 6.0f}},
    };
    std::vector<std::vector<sim::Vec2>> serial;
    serial.reserve(queries.size());
    for (const auto &[start, goal] : queries)
        serial.push_back(grid.find_path(start, goal));

    sim::JobSystem query_jobs(4);
    std::vector<sim::PathScratch> scratch(query_jobs.worker_count());
    for (auto &item : scratch)
        item.resize(static_cast<size_t>(grid.Width) * grid.Height);
    std::vector<std::vector<sim::Vec2>> parallel(queries.size());
    query_jobs.parallel_for(
        queries.size(),
        1,
        sim::JobPriority::Normal,
        [&](size_t index, size_t worker_index) {
            auto &item = scratch[worker_index % scratch.size()];
            parallel[index] = grid.find_path(
                queries[index].first,
                queries[index].second,
                item
            );
        }
    );
    query_jobs.wait_idle();
    for (size_t i = 0; i < queries.size(); ++i)
        assert_paths_equal(serial[i], parallel[i]);

    sim::JobSystem pipeline_jobs(2);
    sim::NavigationPipeline pipeline(pipeline_jobs);
    auto shared_grid = std::make_shared<sim::NavGrid>(grid);
    pipeline.set_nav_grid(shared_grid);
    sim::NavigationRequest request;
    request.Requester = entt::entity{42};
    request.Start = {-8.0f, -8.0f};
    request.Goal = {8.0f, 8.0f};
    request.NavRevision = shared_grid->Revision;
    request.SubmitTick = 10;
    uint64_t request_id = 0;
    assert(pipeline.submit(request, request_id));
    std::vector<sim::NavigationResult> results;
    pipeline.poll_completed(10, results);
    assert(results.empty());
    pipeline_jobs.wait_idle();
    pipeline.poll_completed(10, results);
    assert(results.empty());
    pipeline.poll_completed(11, results);
    assert(results.size() == 1);
    assert(results.front().Request.RequestId == request_id);
    assert(results.front().Status == sim::NavigationResultStatus::Success);
    assert(!results.front().Waypoints.empty());
    const auto navigation_metrics = pipeline.metrics();
    assert(navigation_metrics.Submitted == 1);
    assert(navigation_metrics.Completed == 1);
    assert(navigation_metrics.ResultAgeTicks == 1);
    assert(navigation_metrics.NoPath == 0);

    // A moving attack target invalidates the completed path before it can be
    // applied, then submits exactly one replacement request.
    entt::registry path_registry;
    sim::StatsConfig path_config;
    path_config.derive();
    path_registry.ctx().emplace<sim::StatsConfig>(path_config);
    auto attacker = path_registry.create();
    auto enemy = path_registry.create();
    path_registry.emplace<sim::HeroTag>(attacker, true);
    path_registry.emplace<sim::Position2D>(attacker, sim::Vec2{-8.0f, -8.0f});
    path_registry.emplace<sim::HeroInputState>(attacker);
    path_registry.emplace<sim::CastState>(attacker);
    path_registry.emplace<sim::MovePath>(attacker);
    path_registry.emplace<sim::PathQueryState>(attacker);
    path_registry.emplace<sim::AttackTarget>(attacker, enemy, 2, false);
    path_registry.emplace<sim::AttackProfile>(
        attacker, sim::AttackDelivery::Melee, 1.0f
    );
    path_registry.emplace<sim::NetworkId>(attacker, 1);
    path_registry.emplace<sim::Dead>(attacker, false);
    path_registry.emplace<sim::Position2D>(enemy, sim::Vec2{8.0f, 8.0f});
    path_registry.emplace<sim::Dead>(enemy, false);

    sim::JobSystem merge_jobs(1);
    sim::NavigationPipeline merge_pipeline(merge_jobs);
    merge_pipeline.set_nav_grid(shared_grid);
    sim::pathfinding_system(path_registry, merge_pipeline, 1, 64, 6);
    merge_jobs.wait_idle();
    path_registry.get<sim::Position2D>(enemy).Value = sim::Vec2{8.0f, -8.0f};
    sim::pathfinding_system(path_registry, merge_pipeline, 2, 64, 6);
    const auto merge_metrics = merge_pipeline.metrics();
    assert(merge_metrics.Submitted == 2);
    assert(merge_metrics.Merged == 1);
    assert(merge_metrics.Stale >= 1);
    assert(path_registry.get<sim::PathQueryState>(attacker).InFlight);

    // NoPath clears the old path and observes the configured retry backoff
    // before the same ground-move intent is submitted again.
    entt::registry retry_registry;
    retry_registry.ctx().emplace<sim::StatsConfig>(path_config);
    auto retry_entity = retry_registry.create();
    retry_registry.emplace<sim::HeroTag>(retry_entity, true);
    retry_registry.emplace<sim::Position2D>(retry_entity, sim::Vec2{-8.0f, 0.0f});
    retry_registry.emplace<sim::HeroInputState>(retry_entity);
    retry_registry.get<sim::HeroInputState>(retry_entity).MoveTarget =
        sim::Vec2{8.0f, 0.0f};
    retry_registry.get<sim::HeroInputState>(retry_entity).MoveIssue = true;
    retry_registry.emplace<sim::CastState>(retry_entity);
    retry_registry.emplace<sim::MovePath>(retry_entity);
    retry_registry.emplace<sim::PathQueryState>(retry_entity);
    retry_registry.emplace<sim::Dead>(retry_entity, false);

    auto unreachable_shared = std::make_shared<sim::NavGrid>(unreachable_grid);
    sim::JobSystem retry_jobs(1);
    sim::NavigationPipeline retry_pipeline(retry_jobs);
    retry_pipeline.set_nav_grid(unreachable_shared);
    sim::pathfinding_system(retry_registry, retry_pipeline, 1, 64, 6);
    retry_jobs.wait_idle();
    sim::pathfinding_system(retry_registry, retry_pipeline, 2, 64, 6);
    assert(!retry_registry.get<sim::MovePath>(retry_entity).Following);
    assert(
        retry_registry.get<sim::PathQueryState>(retry_entity).RetryAfterTick ==
        8
    );
    sim::pathfinding_system(retry_registry, retry_pipeline, 7, 64, 6);
    assert(retry_pipeline.metrics().Submitted == 1);
    sim::pathfinding_system(retry_registry, retry_pipeline, 8, 64, 6);
    assert(retry_pipeline.metrics().Submitted == 2);
}

} // namespace

int main() {
    sim::StatsConfig config;
    std::string error;
    const std::string yaml = load_fixture();
    assert(!yaml.empty());
    assert(sim::load_stats_yaml(yaml, config, error));
    assert(error.empty());
    assert(config.Heroes.size() == 2);
    assert(std::abs(config.SnapshotRate - 20.0f) < 0.0001f);
    assert(config.JobWorkerThreads == 0);
    assert(config.PathMaxSubmissionsPerTick == 64);
    assert(config.PathFailureRetryTicks == 6);
    assert(std::abs(config.AttackChaseRepathDeadzoneSq - 2.25f) < 0.0001f);

    sim::StatsConfig invalid_config;
    std::string invalid_error;
    std::string duplicate_hero = yaml;
    assert(replace_once(duplicate_hero, "        id: 2", "        id: 1"));
    assert(!sim::load_stats_yaml(duplicate_hero, invalid_config, invalid_error));
    std::string unknown_kind = yaml;
    assert(replace_once(unknown_kind, "kind: melee_single", "kind: unknown"));
    invalid_error.clear();
    assert(!sim::load_stats_yaml(unknown_kind, invalid_config, invalid_error));
    std::string unknown_reference = yaml;
    assert(replace_once(unknown_reference, "skill_ids: [5, 6, 7, 8]", "skill_ids: [5, 6, 7, 99]"));
    invalid_error.clear();
    assert(!sim::load_stats_yaml(unknown_reference, invalid_config, invalid_error));

    sim::register_builtin_heroes(config);
    sim::register_builtin_skills(config);
    const auto *bloodreaver = sim::HeroRegistry::instance().find(2);
    assert(bloodreaver != nullptr);
    assert(bloodreaver->Name == "Bloodreaver");
    assert(bloodreaver->BaseHp == 140);
    assert(bloodreaver->AttackType == sim::AttackDelivery::Melee);
    assert(bloodreaver->AttackRange == 2.2f);
    assert(sim::SkillRegistry::instance().get(5)->target_mode() ==
           sim::SkillTargetMode::Self);
    assert(sim::SkillRegistry::instance().get(6)->target_mode() ==
           sim::SkillTargetMode::Unit);
    assert(sim::SkillRegistry::instance().get(7)->target_mode() ==
           sim::SkillTargetMode::Self);
    assert(sim::SkillRegistry::instance().get(8)->is_passive());
    assert(sim::SkillRegistry::instance().get(8)->max_level() == 1);

    entt::registry registry;
    registry.ctx().emplace<sim::StatsConfig>(config);
    auto entity = registry.create();
    registry.emplace<sim::Health>(entity, 70, 140);
    registry.emplace<sim::TimedModifiers>(entity);
    sim::apply_timed_modifier(
        registry,
        entity,
        sim::TimedModifierType::MoveSpeedMultiplier,
        5,
        1.4f,
        5.0f
    );
    sim::apply_timed_modifier(
        registry,
        entity,
        sim::TimedModifierType::AttackSpeedMultiplier,
        6,
        1.5f,
        3.0f
    );
    assert(std::abs(sim::effective_move_speed(registry, entity, 5.2f) - 7.28f) < 0.001f);
    assert(std::abs(sim::effective_attack_speed(registry, entity, 0.9f) - 1.35f) < 0.001f);
    assert(std::abs(sim::health_missing_ratio(registry, entity) - 0.5f) < 0.001f);

    registry.emplace<sim::BasicAttackPassive>(entity, 0.1f, 0.5f, 0.1f, 0.5f, 2.0f);
    assert(std::abs(sim::current_lifesteal(registry, entity) - 0.3f) < 0.001f);
    registry.get<sim::Health>(entity).Cur = 0;
    assert(std::abs(sim::current_lifesteal(registry, entity) - 0.5f) < 0.001f);
    sim::timed_modifier_system(registry, 3.0f);
    assert(!sim::has_timed_modifier(
        registry, entity, sim::TimedModifierType::AttackSpeedMultiplier
    ));

    // Q: self cast applies movement speed and terrain immunity, then expires.
    entt::registry skill_registry;
    skill_registry.ctx().emplace<sim::StatsConfig>(config);
    auto caster = skill_registry.create();
    skill_registry.emplace<sim::Position2D>(caster, sim::Vec2{0.0f, 0.0f});
    skill_registry.emplace<sim::Health>(caster, 140, 140);
    skill_registry.emplace<sim::MoveSpeed>(caster, 5.2f);
    skill_registry.emplace<sim::TimedModifiers>(caster);
    sim::CommandBuffer commands;
    sim::IdState ids;
    auto *ghost_rush = sim::SkillRegistry::instance().get(5);
    assert(ghost_rush != nullptr);
    sim::CastState rush_state;
    ghost_rush->on_cast_complete(
        skill_registry, caster, rush_state, commands, ids, 1
    );
    assert(sim::has_timed_modifier(
        skill_registry, caster, sim::TimedModifierType::IgnoreTerrain
    ));
    assert(std::abs(sim::effective_move_speed(skill_registry, caster, 5.2f) -
                    7.28f) < 0.001f);
    sim::timed_modifier_system(skill_registry, 5.0f);
    assert(!sim::has_timed_modifier(
        skill_registry, caster, sim::TimedModifierType::IgnoreTerrain
    ));

    // W: the target's current position is used and the attack-speed buff is timed.
    auto target = skill_registry.create();
    skill_registry.emplace<sim::Position2D>(target, sim::Vec2{4.0f, 0.0f});
    skill_registry.emplace<sim::Health>(target, 100, 100);
    skill_registry.emplace<sim::Damageable>(target);
    skill_registry.emplace<sim::NetworkId>(target, 2);
    skill_registry.emplace<sim::Dead>(target, false);
    skill_registry.get<sim::Position2D>(target).Value = sim::Vec2{6.0f, 1.0f};
    sim::CastState bloodstep_state;
    bloodstep_state.TargetEntity = target;
    bloodstep_state.TargetNetworkId = 2;
    auto *bloodstep = sim::SkillRegistry::instance().get(6);
    assert(bloodstep != nullptr);
    skill_registry.emplace<sim::NetworkId>(caster, 1);
    skill_registry.emplace<sim::CombatStats>(caster, 14.0f, 0.9f, -999.0);
    bloodstep->on_cast_complete(
        skill_registry, caster, bloodstep_state, commands, ids, 1
    );
    auto caster_pos = skill_registry.get<sim::Position2D>(caster).Value;
    auto target_pos = skill_registry.get<sim::Position2D>(target).Value;
    assert(std::abs(caster_pos.x - target_pos.x) < 0.001f);
    assert(std::abs(caster_pos.y - target_pos.y) < 0.001f);
    assert(std::abs(sim::effective_attack_speed(skill_registry, caster, 0.9f) -
                    1.35f) < 0.001f);

    // E: include the exact radius boundary, exclude the caster, and expire after 3s.
    skill_registry.get<sim::Position2D>(caster).Value = sim::Vec2{0.0f, 0.0f};
    auto edge_target = skill_registry.create();
    skill_registry.emplace<sim::Position2D>(edge_target, sim::Vec2{5.0f, 0.0f});
    skill_registry.emplace<sim::Health>(edge_target, 100, 100);
    skill_registry.emplace<sim::Damageable>(edge_target);
    skill_registry.emplace<sim::MoveSpeed>(edge_target, 5.0f);
    skill_registry.emplace<sim::TimedModifiers>(edge_target);
    auto outside_target = skill_registry.create();
    skill_registry.emplace<sim::Position2D>(outside_target, sim::Vec2{5.01f, 0.0f});
    skill_registry.emplace<sim::Health>(outside_target, 100, 100);
    skill_registry.emplace<sim::Damageable>(outside_target);
    skill_registry.emplace<sim::MoveSpeed>(outside_target, 5.0f);
    skill_registry.emplace<sim::TimedModifiers>(outside_target);
    auto *pulse = sim::SkillRegistry::instance().get(7);
    assert(pulse != nullptr);
    sim::CastState pulse_state;
    pulse->on_cast_complete(
        skill_registry, caster, pulse_state, commands, ids, 1
    );
    assert(std::abs(sim::effective_move_speed(skill_registry, edge_target, 5.0f) -
                    3.0f) < 0.001f);
    assert(std::abs(sim::effective_move_speed(skill_registry, outside_target, 5.0f) -
                    5.0f) < 0.001f);
    assert(std::abs(sim::effective_move_speed(skill_registry, caster, 5.2f) -
                    5.2f) < 0.001f);
    sim::timed_modifier_system(skill_registry, 3.0f);
    assert(std::abs(sim::effective_move_speed(skill_registry, edge_target, 5.0f) -
                    5.0f) < 0.001f);

    // R/basic attack: deterministic critical damage, actual-damage lifesteal, and events.
    auto attack_target = skill_registry.create();
    skill_registry.emplace<sim::Position2D>(attack_target, sim::Vec2{0.0f, 0.0f});
    skill_registry.emplace<sim::Health>(attack_target, 20, 20);
    skill_registry.emplace<sim::Damageable>(attack_target);
    skill_registry.emplace<sim::NetworkId>(attack_target, 3);
    skill_registry.emplace<sim::Dead>(attack_target, false);
    auto event_entity = skill_registry.create();
    skill_registry.emplace<sim::AttackStartedEventBuffer>(event_entity);
    skill_registry.emplace<sim::ImpactEventBuffer>(event_entity);
    auto &attack_events = skill_registry.get<sim::AttackStartedEventBuffer>(event_entity);
    skill_registry.emplace<sim::BasicAttackPassive>(caster, 0.1f, 0.5f, 1.0f, 1.0f, 2.0f);
    skill_registry.get<sim::Health>(caster).Cur = 70;
    skill_registry.get<sim::CombatStats>(caster).LastFireTime = -999.0;
    std::mt19937 attack_rng(7);
    assert(sim::try_basic_attack(
        skill_registry, caster, attack_target, 1.0, attack_rng
    ));
    assert(skill_registry.get<sim::Health>(attack_target).Cur == 0);
    assert(skill_registry.get<sim::Health>(caster).Cur == 76);
    assert(attack_events.events.size() == 1);
    auto &impact_events = skill_registry.get<sim::ImpactEventBuffer>(event_entity);
    assert(impact_events.events.size() == 1);
    assert(impact_events.events.front().Damage == 20);
    assert(impact_events.events.front().Healing == 6);
    assert(impact_events.events.front().Critical);

    test_job_system_and_navigation();

    return 0;
}
