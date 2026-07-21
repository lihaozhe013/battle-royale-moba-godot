#include "skill_registry.h"
#include "melee_strike.h"
#include "aoe_field.h"
#include "dash.h"
#include "channel_burst.h"

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

bool SkillRegistry::has(int id) const { return _skills.find(id) != _skills.end(); }

void register_builtin_skills() {
    auto &r = SkillRegistry::instance();
    r.register_skill(1, std::make_unique<MeleeStrikeSkill>());
    r.register_skill(2, std::make_unique<AoEFieldSkill>());
    r.register_skill(3, std::make_unique<DashSkill>());
    r.register_skill(4, std::make_unique<ChannelBurstSkill>());
}

} // namespace sim
