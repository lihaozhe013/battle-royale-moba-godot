#pragma once

#include "../stats_config.h"
#include "skill_interface.h"
#include <memory>
#include <unordered_map>

namespace sim {

class SkillRegistry {
  public:
    static SkillRegistry &instance();

    void register_skill(int id, std::unique_ptr<ISkill> skill);
    ISkill *get(int id) const;
    bool has(int id) const;
    void clear();

  private:
    SkillRegistry() = default;
    std::unordered_map<int, std::unique_ptr<ISkill>> _skills;
};

void register_builtin_skills(const StatsConfig &config);

} // namespace sim
