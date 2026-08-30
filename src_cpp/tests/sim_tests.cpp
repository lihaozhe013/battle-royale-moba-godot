#include "../sim/components.h"
#include "../sim/game_config.h"
#include "../sim/heroes/hero_registry.h"
#include "../sim/skills/skill_registry.h"
#include "../sim/stats_config.h"
#include "../sim/systems/attack_fire.h"
#include "../sim/systems/timed_modifiers.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

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

} // namespace

int main() {
    sim::StatsConfig config;
    std::string error;
    const std::string yaml = load_fixture();
    assert(!yaml.empty());
    assert(sim::load_stats_yaml(yaml, config, error));
    assert(error.empty());
    assert(config.Heroes.size() == 2);

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

    return 0;
}
