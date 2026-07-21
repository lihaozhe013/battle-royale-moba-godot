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

void HeroRegistry::register_hero(const HeroDef &def) {
    _heroes[def.Id] = def;
}

void register_builtin_heroes() {
    auto &r = HeroRegistry::instance();
    r.register_hero({
        .Id = 1,
        .Name = "Swordsman",
        .SkillIds = {1, 2, 3, 4},
        .BaseHp = 100,
        .BaseMana = 300.0f,
        .BaseAtk = 10.0f,
        .BaseAsp = 1.0f,
        .BaseMoveSpeed = 5.0f,
        .AttackRange = 8.0f,
        .HpPerLevel = 10.0f,
        .PrefabId = 0,
    });
}

} // namespace sim
