#include "hero_registry.h"

namespace sim {

HeroRegistry &HeroRegistry::instance() {
    static HeroRegistry inst;
    return inst;
}

const HeroDef &HeroRegistry::get(int id) const {
    static HeroDef fallback;
    auto it = _heroes.find(id);
    return it != _heroes.end() ? it->second : fallback;
}

void HeroRegistry::register_hero(const HeroDef &def) { _heroes[def.Id] = def; }

void register_builtin_heroes(const StatsConfig &config) {
    auto &r = HeroRegistry::instance();
    for (const auto &hero : config.Heroes)
        r.register_hero(hero);
}

} // namespace sim
