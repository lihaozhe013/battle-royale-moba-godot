#pragma once

#include <string>

namespace sim {

struct HeroDef {
    int Id = 0;
    std::string Name;

    int SkillIds[4] = {0, 0, 0, 0};

    int BaseHp = 100;
    float BaseMana = 300.0f;
    float BaseAtk = 10.0f;
    float BaseAsp = 1.0f;
    float BaseMoveSpeed = 5.0f;
    float AttackRange = 8.0f;

    float HpPerLevel = 10.0f;
    float AtkPerLevel = 1.0f;
    float AspPerLevel = 0.03f;
    float SpeedPerLevel = 0.5f;

    int PrefabId = 0;
};

} // namespace sim
