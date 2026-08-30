#include "skill_registry.h"
#include "aoe_field.h"
#include "channel_burst.h"
#include "dash.h"
#include "low_health_passive.h"
#include "melee_strike.h"
#include "radial_slow.h"
#include "target_teleport.h"
#include "terrain_rush.h"

namespace sim {

SkillRegistry &SkillRegistry::instance() {
    static SkillRegistry inst;
    return inst;
}

void SkillRegistry::register_skill(int id, std::unique_ptr<ISkill> skill) {
    _skills[id] = std::move(skill);
}

ISkill *SkillRegistry::get(int id) const {
    auto it = _skills.find(id);
    return it != _skills.end() ? it->second.get() : nullptr;
}

bool SkillRegistry::has(int id) const {
    return _skills.find(id) != _skills.end();
}

void SkillRegistry::clear() { _skills.clear(); }

void register_builtin_skills(const StatsConfig &config) {
    auto &r = SkillRegistry::instance();
    r.clear();
    for (const auto &[id, tuning] : config.Skills) {
        std::unique_ptr<ISkill> skill;
        switch (tuning.Kind) {
        case SkillKind::MeleeSingle:
            skill = std::make_unique<MeleeStrikeSkill>(tuning);
            break;
        case SkillKind::AoEField:
            skill = std::make_unique<AoEFieldSkill>(tuning);
            break;
        case SkillKind::Dash:
            skill = std::make_unique<DashSkill>(tuning);
            break;
        case SkillKind::ChannelBurst:
            skill = std::make_unique<ChannelBurstSkill>(tuning);
            break;
        case SkillKind::TerrainRush:
            skill = std::make_unique<TerrainRushSkill>(tuning);
            break;
        case SkillKind::TargetTeleport:
            skill = std::make_unique<TargetTeleportSkill>(tuning);
            break;
        case SkillKind::RadialSlow:
            skill = std::make_unique<RadialSlowSkill>(tuning);
            break;
        case SkillKind::LowHealthPassive:
            skill = std::make_unique<LowHealthPassiveSkill>(tuning);
            break;
        }
        if (!skill || skill->id() != id)
            continue;
        r.register_skill(id, std::move(skill));
    }
}

} // namespace sim
