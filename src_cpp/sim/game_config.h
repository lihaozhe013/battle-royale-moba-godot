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

    static constexpr int BotCount = 5;
    static constexpr float BotRadius = 0.5f;
    static constexpr float BotSpeed = 2.0f;
    static constexpr int BotHp = 50;
    static constexpr float BotBaseAttack = 5.0f;
    static constexpr float BotBaseAttackSpeed = 0.8f;
    static constexpr float BotRespawnTime = 3.0f;
    static constexpr float BotVisionRange = 20.0f;
    static constexpr float BotWanderIntervalMin = 2.0f;
    static constexpr float BotWanderIntervalMax = 5.0f;

    static constexpr int PlayerIdStart = 1;
    static constexpr int BotIdStart = 1001;
    static constexpr int ArrowIdStart = 2001;
    static constexpr int PickupIdStart = 3001;

    static constexpr int XpPerLevelBase = 100;
    static constexpr int HpPerLevel = 10;
    static constexpr float SpeedPerLevel = 0.5f;
    static constexpr float HealFraction = 0.3f;

    static constexpr int XpPickupValue = 20;
    static constexpr int HealPickupValue = 30;
    static constexpr int SmallHealPickupValue = 25;
    static constexpr float XpPickupRespawnTime = 8.0f;
    static constexpr float HealPickupRespawnTime = 12.0f;
    static constexpr float SmallHealPickupRespawnTime = 8.0f;
    static constexpr float PickupRadius = 0.5f;

    static constexpr int XpPickupCount = 6;
    static constexpr int HealPickupCount = 3;
    static constexpr int SmallHealPickupCount = 5;
};

} // namespace sim
