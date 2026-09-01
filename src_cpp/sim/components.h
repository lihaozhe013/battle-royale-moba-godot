#pragma once

#include "vec2.h"
#include <cstdint>
#include <entt/entt.hpp>
#include <vector>

namespace sim {

// ── Skill enum forward declarations ──

enum class SkillKind : uint8_t {
    MeleeSingle = 0,  // C
    AoEField = 1,     // E
    Dash = 2,         // R
    ChannelBurst = 3, // F
    TerrainRush = 4,
    TargetTeleport = 5,
    RadialSlow = 6,
    LowHealthPassive = 7,
};

enum class SkillTargetMode : uint8_t {
    Self = 0,
    Point = 1,
    Unit = 2,
    Passive = 3,
};

// Short alias used by gameplay-facing code and future skill definitions.
using TargetMode = SkillTargetMode;

enum class AttackDelivery : uint8_t {
    Projectile = 0,
    Melee = 1,
};

enum class TimedModifierType : uint8_t {
    MoveSpeedMultiplier = 0,
    AttackSpeedMultiplier = 1,
    IgnoreTerrain = 2,
};

enum class StatusType : uint8_t {
    None = 0,
    Root = 1, // 禁锢 — 不能移动，但可以攻击/放技能
    Stun = 2, // 眩晕 — 不能移动，不能攻击，不能放技能
};

enum class MatchResult : uint8_t {
    InProgress = 0,
    Defeat = 1,
    Victory = 2,
};

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

struct Mana {
    float Cur = 0.0f;
    float Max = 0.0f;
    float RegenRate = 0.0f;
    float RegenDelay = 3.0f;
    float RegenTimer = 0.0f;
};

struct Lifetime {
    float Remaining = 0.0f;
};

struct NetworkId {
    int Value = 0;
};

struct Damageable {};

// ── HeroComponents ──

struct HeroTag {
    bool IsLocal = false;
};

struct HeroInputState {
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;

    int SkillSlot = -1;
    bool SkillConfirm = false;
    Vec2 SkillAim{0.0f};
    int SkillTargetId = -1;
    int SkillUpgradeSlot = -1;

    bool CancelSkill = false;
    bool CancelAttack = false;

    int AttackTargetId = -1;
    bool AttackGround = false;
    Vec2 AttackGroundPos{0.0f};
    bool AttackClear = false;

    int Seq = 0;
};

struct HeroDefId {
    int Value = 0;
};

struct AttackProfile {
    AttackDelivery Delivery = AttackDelivery::Projectile;
    float Range = 8.0f;
};

struct TimedModifier {
    TimedModifierType Type = TimedModifierType::MoveSpeedMultiplier;
    int SourceId = 0;
    float Magnitude = 1.0f;
    float Remaining = 0.0f;
};

struct TimedModifiers {
    std::vector<TimedModifier> Values;
};

struct BasicAttackPassive {
    float LifestealMin = 0.0f;
    float LifestealMax = 0.0f;
    float CritChanceMin = 0.0f;
    float CritChanceMax = 0.0f;
    float CritMultiplier = 1.0f;
};

// ── 过渡期别名（v2 → v3 过渡，全部完成迁移后删除） ──
using PlayerTag = HeroTag;
using PlayerInputState = HeroInputState;

// ── PlayerComponents.cs（过渡，删除中） ──

struct CombatStats {
    float Atk = 0.0f;
    float Asp = 0.0f;
    double LastFireTime = 0.0;
};

struct Kills {
    int Value = 0;
};

struct MatchStats {
    int Deaths = 0;
    int DamageDealt = 0;
    int DamageTaken = 0;
    int HealingDone = 0;
    int XpEarned = 0;
    int SkillCasts = 0;
};

// ── BotComponents.cs ─────────────────────────────────────────────────────

struct BotTag {};

struct BotCastRequest {
    int TargetSlot = -1;
    Vec2 AimPos{0.0f};
    int TargetNetworkId = -1;
    bool Valid = false;
    float Score = 0.0f;
};

enum class BotTier : uint8_t {
    Normal = 0,
    Elite = 1,
    Boss = 2,
};

enum class BotRole : uint8_t {
    Fodder = 0,
    Stalker = 1,
    Brute = 2,
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

    enum class KiteSub : uint8_t { Chase, Strafe, Retreat };
    int StrafeDir = 1;
    KiteSub Kite = KiteSub::Strafe;
    float GoalCommitTimer = 0.0f;
};

struct BotAIState {
    Vec2 MoveTarget{0.0f};
    float RespawnTimer = 0.0f;
    entt::entity TargetEntity = entt::null;
    float WanderTimer = 0.0f;
    float TargetLockTimer = 0.0f;
};

struct BotVisionRange {
    float Value = 0.0f;
};

struct BotCombatState {
    enum class Phase : uint8_t {
        Approach = 0,
        Kite = 1,
        Burst = 2,
        Sustain = 3,
        Disengage = 4,
    };
    Phase Current = Phase::Approach;
    float PhaseTimer = 0.0f;
    int BurstStep = 0;
    float BurstTimer = 0.0f;
    float SkillDecisionTimer = 0.0f;
};

// ── StatusEffect.cs ──────────────────────────────────────────────────────

struct StatusEffect {
    StatusType Type = StatusType::None;
    float Timer = 0.0f;
};

struct CastState {
    enum class Phase : uint8_t {
        None = 0,
        Aiming = 1,     // 仅 quick cast 同 tick 中转
        Chasing = 2,    // 跟随施法：confirm 超范围，A* 朝目标移动
        Casting = 3,    // 前摇
        Channeling = 4, // 引导（F）
        Dashing = 5,    // 位移（R）
    };
    Phase State = Phase::None;
    int ActiveSlot = -1;
    int SkillId = 0;
    float Timer = 0.0f;
    float SubTimer = 0.0f;
    float RejectTimer = 0.0f; // cooldown after None to prevent re-entry
    float PendingCooldown = 0.0f;
    float PendingManaCost = 0.0f;
    Vec2 AimPos{0.0f};
    Vec2 DashStart{0.0f};
    Vec2 DashTarget{0.0f};
    int HitTargetId = -1;
    int CastError = 0;
    entt::entity TargetEntity = entt::null;
    int TargetNetworkId = -1; // 指向性技能锁定目标 NetworkId
    bool QuickCast = false;   // 标记本次施法来源（View 触发 quick vs normal）
};

// ── AoE components ──

struct AoETag {
    int OwnerId = 0;
    int SkillId = 0;
    float Radius = 0.0f;
    float Duration = 0.0f;
    float Timer = 0.0f;
};

// ── ArrowComponents.cs ───────────────────────────────────────────────────

struct ArrowTag {
    int OwnerId = 0;
    entt::entity OwnerEntity = entt::null;
    float Dmg = 0.0f;
    float LifestealRatio = 0.0f;
    int SourceSkillId = 0;
    bool Critical = false;
};

struct AttackTarget {
    entt::entity Target = entt::null;
    int TargetNetworkId = -1;
    bool Chasing =
        false; // 每 tick 由 player_movement 设置，wall_collision 跳过
};

struct Homing {
    entt::entity Target = entt::null;
    int TargetNetId = -1;
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

// ── SkillComponents.cs ──────────────────────────────────────────────────

struct SkillSlot {
    int SkillId = 0;
    int Level = 1;
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
    float ManaCost = 0.0f;
};

struct SkillComponent {
    SkillSlot
        Slots[4]; // [0-3] QWER 技能, 普攻虚拟槽已移除（走独立 ATTACK 命令）
};

// ── SkillPoints (新增, v1 完全缺失) ──
struct SkillPoints {
    int Available = 0;
};

// ── MovePath.cs ──────────────────────────────────────────────────────────

struct MovePath {
    std::vector<Vec2> Waypoints;
    int CurrentIndex = 0;
    bool Following = false;
    Vec2 FinalTarget{0.0f};
};

enum class PathQueryChannel : uint8_t {
    Movement = 0,
    Skill = 1,
    Effect = 2,
};

enum class PathQueryPriority : uint8_t {
    High = 0,
    Normal = 1,
    Low = 2,
};

enum class PathQueryDestination : uint8_t {
    Ground = 0,
    Entity = 1,
    Point = 2,
};

// Main-thread intent and in-flight bookkeeping for an asynchronous navigation
// query. Worker jobs only receive a value copy of the request and never touch
// this component or the registry.
struct PathQueryState {
    PathQueryChannel Channel = PathQueryChannel::Movement;
    PathQueryPriority Priority = PathQueryPriority::Normal;
    PathQueryDestination Destination = PathQueryDestination::Ground;
    entt::entity TargetEntity = entt::null;
    Vec2 DesiredTarget{0.0f};
    uint64_t IntentVersion = 0;
    uint64_t InFlightVersion = 0;
    uint64_t InFlightRequestId = 0;
    int RetryAfterTick = 0;
    bool HasIntent = false;
    bool InFlight = false;
    bool Dirty = false;
};

// ── SingletonComponents.cs ───────────────────────────────────────────────

struct LocalInputSingleton {
    // ── 移动 ──
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;

    // ── 技能 ──
    int SkillSlot = -1; // 当前 Aiming 槽，-1=无（0-3 QWER, 10-15 装备预留）
    bool SkillConfirm = false; // 本 tick 是否确认
    Vec2 SkillAim{0.0f};
    int SkillTargetId = -1;
    int SkillUpgradeSlot = -1; // 技能升级脉冲（Ctrl+QWER），-1=无

    // ── 施法取消 ──
    bool CancelSkill = false;
    bool CancelAttack = false;

    // ── 普攻 ──
    int AttackTargetId = -1;
    bool AttackGround = false;
    Vec2 AttackGroundPos{0.0f};
    bool AttackClear = false;

    // ── 序号 ──
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

struct ImpactEvent {
    int AttackerId = 0;
    int VictimId = 0;
    int SourceSkillId = 0;
    int Damage = 0;
    int Healing = 0;
    bool Critical = false;
};

struct ImpactEventBuffer {
    std::vector<ImpactEvent> events;
};

struct AttackStartedEvent {
    int AttackerId = 0;
    int TargetId = -1;
};

struct AttackStartedEventBuffer {
    std::vector<AttackStartedEvent> events;
};

// ── IdState.cs ───────────────────────────────────────────────────────────

struct IdState {
    int NextPlayerId = 0;
    int NextBotId = 0;
    int NextArrowId = 0;
    int NextPickupId = 0;
    int NextAoEId = 0;
};

} // namespace sim
