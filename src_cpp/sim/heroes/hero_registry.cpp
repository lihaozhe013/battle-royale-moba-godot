#include "hero_registry.h"

namespace sim {

HeroRegistry &HeroRegistry::instance() {
    static HeroRegistry inst;
    return inst;
}

const HeroDef *HeroRegistry::find(int id) const {
    auto it = _heroes.find(id);
    return it != _heroes.end() ? &it->second : nullptr;
}

const HeroDef &HeroRegistry::get(int id) const {
    static HeroDef fallback;
    const HeroDef *hero = find(id);
    return hero ? *hero : fallback;
}

void HeroRegistry::clear() { _heroes.clear(); }

void HeroRegistry::register_hero(const HeroDef &def) { _heroes[def.Id] = def; }

void register_builtin_heroes(const StatsConfig &config) {
    auto &r = HeroRegistry::instance();
    r.clear();
    for (const auto &hero : config.Heroes)
        r.register_hero(hero);
}

} // namespace sim
