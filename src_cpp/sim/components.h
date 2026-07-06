#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>
#include "vec2.h"

namespace sim {

// ── CommonComponents.cs ──────────────────────────────────────────────────

struct Position2D {
    Vec2 Value;
};

struct Velocity2D {
    Vec2 Value;
};

struct FacingAngle {
    float Radians = 0.0f;
};

struct Health {
    int Cur = 0;
    int Max = 0;
};

struct Dead {
    bool enabled = false;
};

struct Lifetime {
    float Remaining = 0.0f;
};

struct NetworkId {
    int Value = 0;
};

struct Damageable {};

// ── PlayerComponents.cs ──────────────────────────────────────────────────

struct PlayerTag {
    bool IsLocal = false;
};

struct PlayerInputState {
    Vec2 Move{0.0f};
    Vec2 Aim{0.0f};
    bool Fire = false;
    int Seq = 0;
};

struct CombatStats {
    float Atk = 0.0f;
    float Asp = 0.0f;
    double LastFireTime = 0.0;
};

struct Kills {
    int Value = 0;
};

// ── BotComponents.cs ─────────────────────────────────────────────────────

struct BotTag {};

enum class BotTier : uint8_t {
    Normal = 0,
    Elite = 1,
    Boss = 2,
};

struct BotBehaviorState {
    enum class Goal : uint8_t {
        Flee = 0,
        SeekHeal = 1,
        SeekXp = 2,
        Engage = 3,
        Wander = 4,
    };
    Goal Current = Goal::Wander;
    entt::entity PickupTarget = entt::null;
    float DecisionCooldown = 0.0f;
};

struct BotAIState {
    Vec2 MoveTarget{0.0f};
    float RespawnTimer = 0.0f;
    entt::entity TargetEntity = entt::null;
    float WanderTimer = 0.0f;
};

struct BotVisionRange {
    float Value = 0.0f;
};

// ── ArrowComponents.cs ───────────────────────────────────────────────────

struct ArrowTag {
    int OwnerId = 0;
    entt::entity OwnerEntity = entt::null;
    float Dmg = 0.0f;
};

// ── WallComponents.cs ────────────────────────────────────────────────────

struct WallTag {};

struct WallBounds {
    Vec2 Min{0.0f};
    Vec2 Max{0.0f};
};

// ── PickupComponents.cs ──────────────────────────────────────────────────

enum class PickupType : uint8_t {
    Xp = 0,
    Heal = 1,
    SmallHeal = 2,
};

struct PickupTag {
    PickupType Type = PickupType::Xp;
    int Value = 0;
};

struct PickupSpawner {
    PickupType Type = PickupType::Xp;
    int Value = 0;
    Vec2 Position{0.0f};
    float RespawnTime = 0.0f;
    float CurrentTimer = 0.0f;
    bool Active = false;
    int CurrentEntityId = 0;
};

struct Level {
    int Value = 0;
};

struct Experience {
    int Cur = 0;
    int Needed = 0;
};

struct MoveSpeed {
    float Value = 0.0f;
};

// ── SingletonComponents.cs ───────────────────────────────────────────────

struct LocalInputSingleton {
    Vec2 Move{0.0f};
    Vec2 Aim{0.0f};
    bool Fire = false;
    int Seq = 0;
};

struct MapBounds {
    float Half = 0.0f;
};

struct KillEvent {
    int KillerId = 0;
    int VictimId = 0;
};

struct KillEventBuffer {
    std::vector<KillEvent> events;
};

// ── IdState.cs ───────────────────────────────────────────────────────────

struct IdState {
    int NextPlayerId = 0;
    int NextBotId = 0;
    int NextArrowId = 0;
    int NextPickupId = 0;
};

} // namespace sim
