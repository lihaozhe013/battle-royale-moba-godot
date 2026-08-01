#include "skill_registry.h"
#include "aoe_field.h"
#include "channel_burst.h"
#include "dash.h"
#include "melee_strike.h"

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

void register_builtin_skills(const StatsConfig &config) {
    auto &r = SkillRegistry::instance();
    r.register_skill(
        1, std::make_unique<MeleeStrikeSkill>(config.Skills.at(1))
    );
    r.register_skill(2, std::make_unique<AoEFieldSkill>(config.Skills.at(2)));
    r.register_skill(3, std::make_unique<DashSkill>(config.Skills.at(3)));
    r.register_skill(
        4, std::make_unique<ChannelBurstSkill>(config.Skills.at(4))
    );
}

} // namespace sim
