#pragma once

#include "../stats_config.h"
#include "hero_def.h"
#include <unordered_map>

namespace sim {

class HeroRegistry {
  public:
    static HeroRegistry &instance();
    const HeroDef &get(int id) const;
    void register_hero(const HeroDef &def);

  private:
    std::unordered_map<int, HeroDef> _heroes;
};

void register_builtin_heroes(const StatsConfig &config);

} // namespace sim
