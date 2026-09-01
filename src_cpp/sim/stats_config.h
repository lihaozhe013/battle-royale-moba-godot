#pragma once

#include "heroes/hero_def.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace sim {

struct SkillTuning {
    int Id = 0;
    std::string Name;
    std::string Description;
    SkillKind Kind = SkillKind::MeleeSingle;
    int MaxLevel = 4;
    float BaseCooldown = 0.0f;
    float BaseManaCost = 0.0f;
    float BaseCastTime = 0.0f;
    float BaseRange = 0.0f;
    float CooldownPerLevel = 0.0f;
    float ManaReductionPerLevel = 0.0f;
    float ManaReductionMin = 0.3f;
    float RangePerLevel = 0.0f;
    float DamageBase = 0.0f;
    float DamagePerLevel = 0.0f;
    float DamageAtkRatio = 0.0f;
    float EffectBase = 0.0f;
    float EffectPerLevel = 0.0f;
    float DashSpeed = 20.0f;
    float ChannelInterval = 0.5f;
    int ChannelProjectileCount = 16;
    float ChannelProjectileSpawnRadius = 0.5f;

    float ModifierMagnitude = 1.0f;
    float ModifierDuration = 0.0f;
    float CritMin = 0.0f;
    float CritMax = 0.0f;
    float LifestealMin = 0.0f;
    float LifestealMax = 0.0f;
    float CritMultiplier = 1.0f;
};

struct StatsConfig {
    float TickRate = 30.0f;
    float SnapshotRate = 20.0f;
    float MapHalf = 50.0f;
    int JobWorkerThreads = 0;

    float PlayerRadius = 0.5f;
    float PlayerSpeed = 5.0f;
    int PlayerBaseHp = 100;
    float BaseAttack = 10.0f;
    float BaseAttackSpeed = 1.0f;
    float AtkPerKill = 2.0f;
    float AspPerKill = 0.05f;
    float AspMax = 4.0f;

    float ArrowSpeed = 20.0f;
    float ArrowLifetime = 2.0f;
    float ArrowRadius = 0.3f;

    int BotCount = 20;
    float BotRadius = 0.5f;
    float BotSpeed = 2.0f;
    int BotHp = 30;
    float BotBaseAttack = 3.0f;
    float BotBaseAttackSpeed = 0.5f;
    float BotRespawnTime = 8.0f;
    float BotVisionRange = 20.0f;
    float BotStatMul = 0.1f;
    float BotWanderIntervalMin = 2.0f;
    float BotWanderIntervalMax = 5.0f;

    int MaxHeroLevel = 30;
    float AtkPerLevel = 1.0f;
    float AspPerLevel = 0.03f;

    float NormalHpMul = 1.0f;
    float NormalAtkMul = 1.0f;
    float NormalAspMul = 1.0f;
    float NormalSpeedMul = 1.0f;
    float NormalVisionMul = 1.0f;
    float EliteHpMul = 2.0f;
    float EliteAtkMul = 1.6f;
    float EliteAspMul = 1.1f;
    float EliteSpeedMul = 1.1f;
    float EliteVisionMul = 1.2f;
    float BossHpMul = 4.0f;
    float BossAtkMul = 2.5f;
    float BossAspMul = 1.25f;
    float BossSpeedMul = 1.2f;
    float BossVisionMul = 1.5f;
    float BossRoll = 0.05f;
    float EliteRoll = 0.20f;

    int FodderMaxLv = 5;
    int BruteMinLv = 22;
    int StalkerOffset = 2;
    int FodderWeight = 4;
    int StalkerWeight = 4;
    int BruteWeight = 2;
    float BruteEliteRoll = 0.6f;

    float BotDecisionCooldown = 0.3f;
    float BotFleeDist = 30.0f;
    float BotEngageRangeHigh = 0.8f;
    float BotEngageRangeLow = 0.3f;
    float BotKiteStrafeDist = 5.0f;
    float BotTargetLockTime = 2.0f;
    float BotGoalCommitTime = 0.8f;
    float BotKiteChaseExit = 0.75f;
    float BotKiteChaseEnter = 0.85f;
    float BotKiteRetreatExit = 0.35f;
    float BotKiteRetreatEnter = 0.25f;

    float BotCombatPhaseCooldown = 0.2f;
    float BotSkillDecisionCooldown = 0.1f;
    float BotGoalDecisionCooldown = 0.5f;
    float BotApproachThreshold = 0.8f;
    float BotKiteThreshold = 1.2f;
    float BotBurstHealthThreshold = 0.7f;
    float BotSustainHealthThreshold = 0.4f;
    float BotDisengageHealthThreshold = 0.3f;
    float BotSafeDistanceMultiplier = 2.0f;
    float BotBurstStepLimit = 3.0f;
    float BotBurstDuration = 2.0f;
    float BotDisengageRecoveryThreshold = 0.6f;

    int KillXpBase = 200;
    float KillXpHighBonus = 0.75f;

    int PlayerIdStart = 1;
    int BotIdStart = 1001;
    int ArrowIdStart = 2001;
    int PickupIdStart = 3001;
    int AoEIdStart = 4001;

    float PlayerSpawnSafeRadius = 12.0f;
    float PlayerSpawnSafeRadiusSq = 144.0f;

    int XpPerLevelBase = 250;
    int HpPerLevel = 10;
    float SpeedPerLevel = 0.5f;
    float HealFraction = 0.5f;

    int XpPickupValue = 48;
    int HealPickupValue = 30;
    int SmallHealPickupValue = 25;
    float XpPickupRespawnTime = 10.0f;
    float HealPickupRespawnTime = 25.0f;
    float SmallHealPickupRespawnTime = 20.0f;
    float PickupRadius = 0.5f;
    int XpPickupCount = 120;
    int HealPickupCount = 2;
    int SmallHealPickupCount = 2;

    float RepathTargetDeadzone = 1.5f;
    float RepathTargetDeadzoneSq = 2.25f;
    float PathTurnRate = 12.0f;
    float SkillChaseRepathDeadzone = 2.0f;
    float SkillChaseRepathDeadzoneSq = 4.0f;
    float AttackChaseRepathDeadzone = 1.5f;
    float AttackChaseRepathDeadzoneSq = 2.25f;
    int PathMaxSubmissionsPerTick = 64;
    int PathFailureRetryTicks = 6;

    float PlayerBaseMana = 300.0f;
    float PlayerManaRegen = 5.0f;
    float BotBaseMana = 80.0f;
    float BotManaRegen = 3.0f;
    float ManaRegenDelay = 3.0f;

    float AttackAcquisitionRange = 15.0f;
    int MaxSkillLevel = 4;
    float SkillManaReductionMin = 0.2f;
    float SkillCDRMin = 0.4f;
    float SkillCDRPerLevel = 0.05f;
    float SkillDamageAtkRatio = 0.9f;

    float BotSkillDmgMul = 0.07f;
    float BotSkillCooldownMul = 1.3f;
    float BotManaCostMul = 0.6f;

    float BotSkillScoreBase = 50.0f;
    float BotSkillScoreInRange = 30.0f;
    float BotSkillScoreNearRange = 10.0f;
    float BotSkillScoreOutOfRange = -20.0f;
    float BotSkillScoreApproachDash = 40.0f;
    float BotSkillScoreApproachMelee = 20.0f;
    float BotSkillScoreKiteAoe = 30.0f;
    float BotSkillScoreKiteChannel = 25.0f;
    float BotSkillScoreBurstMelee = 50.0f;
    float BotSkillScoreBurstAoe = 40.0f;
    float BotSkillScoreSustainChannel = 45.0f;
    float BotSkillScoreSustainAoe = 20.0f;
    float BotSkillScoreDisengageDash = 60.0f;
    float BotSkillScoreLowHealthDash = 50.0f;
    float BotSkillScoreHighHealthChannel = 20.0f;
    float BotSkillScoreLowHealthChannel = -30.0f;
    float BotSkillScoreAoeEnemy = 25.0f;
    float BotSkillEnemyScanRadiusSq = 100.0f;
    float BotSkillLowHealth = 0.3f;
    float BotSkillHighHealth = 0.7f;
    float BotSkillSustainHealth = 0.5f;

    std::vector<HeroDef> Heroes;
    std::unordered_map<int, SkillTuning> Skills;

    void derive() {
        PlayerSpawnSafeRadiusSq = PlayerSpawnSafeRadius * PlayerSpawnSafeRadius;
        RepathTargetDeadzoneSq = RepathTargetDeadzone * RepathTargetDeadzone;
        SkillChaseRepathDeadzoneSq =
            SkillChaseRepathDeadzone * SkillChaseRepathDeadzone;
        AttackChaseRepathDeadzoneSq =
            AttackChaseRepathDeadzone * AttackChaseRepathDeadzone;
    }
};

bool load_stats_yaml(
    const std::string &text, StatsConfig &config, std::string &error
);

} // namespace sim
