#pragma once

#include "components.h"

namespace sim {

struct SkillDef {
    int Id = 0;
    SkillKind Kind;
    float CastTime = 0.0f;
    float ManaCost = 0.0f;
    float ManaReductionPerLevel = 0.05f;
    float Cooldown = 0.0f;
    float Damage = 0.0f;
    float Range = 0.0f;       // C=melee radius; E=aoe radius; R=dash distance
    float EffectValue = 0.0f; // E=root duration; F=channel duration
    float ProjectileSpeed = 0.0f;
    float ChannelTick = 0.0f; // F=interval between bullet waves
    int BulletCount = 0;      // F=bullets per wave
};

inline const SkillDef &get_skill_def(int id) {
    // clang-format off
    // @table
    static const SkillDef table[] = {
        {0, SkillKind::MeleeSingle,  0.0f,   0.0f, 0.05f,  0.0f,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f,  0},  // [0] placeholder
        {1, SkillKind::MeleeSingle,  0.2f,  20.0f, 0.05f,  5.0f, 40.0f, 8.0f, 0.0f,  0.0f, 0.0f,  0},  // C
        {2, SkillKind::AoEField,     0.5f, 100.0f, 0.10f, 20.0f, 35.0f, 4.0f, 2.0f,  0.0f, 0.0f,  0},  // E
        {3, SkillKind::Dash,         0.2f,  40.0f, 0.05f, 10.0f,  0.0f, 8.0f, 0.0f,  0.0f, 0.0f,  0},  // R
        {4, SkillKind::ChannelBurst, 1.0f, 230.0f, 0.03f, 60.0f,  0.0f, 0.0f, 5.0f, 20.0f, 0.5f, 16},  // F
    };
    // clang-format on
    return table[id];
}

} // namespace sim
