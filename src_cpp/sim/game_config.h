#pragma once

namespace sim {

struct GameConfig {
    static constexpr float TickRate = 30.0f;
    static constexpr float SnapshotRate = 20.0f;

    static constexpr float MapHalf = 50.0f;

    static constexpr float PlayerRadius = 0.5f;
    static constexpr float PlayerSpeed = 5.0f;
    static constexpr int PlayerBaseHp = 100;

    static constexpr float BaseAttack = 10.0f;
    static constexpr float BaseAttackSpeed = 1.0f;
    static constexpr float AtkPerKill = 2.0f;
    static constexpr float AspPerKill = 0.05f;
    static constexpr float AspMax = 4.0f;

    static constexpr float ArrowSpeed = 20.0f;
    static constexpr float ArrowLifetime = 2.0f;
    static constexpr float ArrowRadius = 0.3f;

    static constexpr int BotCount = 20;
    static constexpr float BotRadius = 0.5f;
    static constexpr float BotSpeed = 2.0f;
    static constexpr int BotHp = 50;
    static constexpr float BotBaseAttack = 5.0f;
    static constexpr float BotBaseAttackSpeed = 0.8f;
	static constexpr float BotRespawnTime = 8.0f;
    static constexpr float BotVisionRange = 20.0f;
    static constexpr float BotWanderIntervalMin = 2.0f;
    static constexpr float BotWanderIntervalMax = 5.0f;

    // ── Bot 等级成长 ──
    static constexpr int MaxBotLevel = 30;
    static constexpr int BotHpPerLevel = 8;
    static constexpr float BotAtkPerLevel = 0.8f;
    static constexpr float BotAspPerLevel = 0.03f;
    static constexpr float BotSpeedPerLevel = 0.3f;

    // ── Bot Tier 倍率 ──
    static constexpr float NormalHpMul = 1.0f;
    static constexpr float NormalAtkMul = 1.0f;
    static constexpr float NormalAspMul = 1.0f;
    static constexpr float NormalSpeedMul = 1.0f;
    static constexpr float NormalVisionMul = 1.0f;
    static constexpr float EliteHpMul = 2.0f;
    static constexpr float EliteAtkMul = 1.6f;
    static constexpr float EliteAspMul = 1.1f;
    static constexpr float EliteSpeedMul = 1.1f;
    static constexpr float EliteVisionMul = 1.2f;
    static constexpr float BossHpMul = 4.0f;
    static constexpr float BossAtkMul = 2.5f;
    static constexpr float BossAspMul = 1.25f;
    static constexpr float BossSpeedMul = 1.2f;
    static constexpr float BossVisionMul = 1.5f;
    static constexpr float BossRoll = 0.05f;
    static constexpr float EliteRoll = 0.20f;

    // ── Bot Role 阶梯 ──
    static constexpr int FodderMaxLv = 5;
    static constexpr int BruteMinLv = 22;
    static constexpr int StalkerOffset = 2;
    static constexpr int FodderWeight = 4;
    static constexpr int StalkerWeight = 4;
    static constexpr int BruteWeight = 2;
    static constexpr float BruteEliteRoll = 0.6f;

    // ── 决策树参数 ──
    static constexpr float BotDecisionCooldown = 0.3f;
    static constexpr float BotFleeDist = 30.0f;
    static constexpr float BotEngageRangeHigh = 0.8f;
    static constexpr float BotEngageRangeLow = 0.3f;
    static constexpr float BotKiteStrafeDist = 5.0f;
    // ── 目标锁定 ──
    static constexpr float BotTargetLockTime = 2.0f;
    // ── Goal 承诺 ──
    static constexpr float BotGoalCommitTime = 0.8f;
    // ── Kiting 滞回阈值（进入/退出双阈值） ──
    static constexpr float BotKiteChaseExit = 0.75f;
    static constexpr float BotKiteChaseEnter = 0.85f;
    static constexpr float BotKiteRetreatExit = 0.35f;
    static constexpr float BotKiteRetreatEnter = 0.25f;

    // ── 击杀 XP ──
    static constexpr int KillXpBase = 200;
    static constexpr float KillXpHighBonus = 0.75f;

    static constexpr int PlayerIdStart = 1;
    static constexpr int BotIdStart = 1001;
    static constexpr int ArrowIdStart = 2001;
    static constexpr int PickupIdStart = 3001;
    static constexpr int AoEIdStart = 4001;

    static constexpr int XpPerLevelBase = 250;
    static constexpr int HpPerLevel = 10;
    static constexpr float SpeedPerLevel = 0.5f;
    static constexpr float HealFraction = 0.5f;

    static constexpr int XpPickupValue = 48;
    static constexpr int HealPickupValue = 30;
    static constexpr int SmallHealPickupValue = 25;
    static constexpr float XpPickupRespawnTime = 10.0f;
    static constexpr float HealPickupRespawnTime = 25.0f;
    static constexpr float SmallHealPickupRespawnTime = 20.0f;
    static constexpr float PickupRadius = 0.5f;

    static constexpr int XpPickupCount = 120;
    static constexpr int HealPickupCount = 2;
    static constexpr int SmallHealPickupCount = 2;

    // ── Mana ──
    static constexpr float PlayerBaseMana = 300.0f;
    static constexpr float PlayerManaRegen = 5.0f;
    static constexpr float BotBaseMana = 80.0f;
    static constexpr float BotManaRegen = 3.0f;
    static constexpr float ManaRegenDelay = 3.0f;

    // ── Serialised ── Skill Definitions ──
    static constexpr int SkillCount = 4;
    // Player test skills: slot 0-3
    static constexpr int PlayerSkillIds[4] = {1, 2, 3, 4};
    static constexpr float SkillCooldowns[4] = {4.0f, 6.0f, 8.0f, 15.0f};
    static constexpr float SkillManaCosts[4] = {10.0f, 20.0f, 30.0f, 50.0f};
    // Bot test skills: slot 0-3 (same for now)
    static constexpr int BotSkillIds[4] = {1, 2, 3, 4};
};

} // namespace sim
