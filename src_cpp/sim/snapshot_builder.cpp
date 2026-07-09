#include "snapshot_builder.h"
#include "components.h"
#include "game_config.h"
#include "skill_defs.h"
#include <chrono>

namespace sim {

godot::Ref<SimSnapshot> SnapshotBuilder::build(entt::registry &reg, int seq) {
    auto snap = godot::Ref<SimSnapshot>(memnew(SimSnapshot));
    snap->seq = seq;
    snap->t = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()
    )
                  .count();

    _build_players(reg, snap);
    _build_bots(reg, snap);
    _build_arrows(reg, snap);
    _build_pickups(reg, snap);
    _build_events(reg, snap);
    _build_aoes(reg, snap);

    return snap;
}

namespace {

godot::Ref<SimSkillSlotSnap> _build_skill_slot(const SkillSlot &slot, int char_level) {
    auto s = godot::Ref<SimSkillSlotSnap>(memnew(SimSkillSlotSnap));
    s->skill_id = slot.SkillId;
    s->level = char_level;
    s->cooldown = slot.CooldownTimer;
    s->max_cooldown = slot.MaxCooldown;
    const auto &def = get_skill_def(slot.SkillId);
    float mana_mult = std::max(
        GameConfig::SkillManaReductionMin,
        1.0f - (char_level - 1) * def.ManaReductionPerLevel
    );
    s->mana_cost = def.ManaCost * mana_mult;
    return s;
}

void _build_skills(
    godot::TypedArray<SimSkillSlotSnap> &arr, const SkillComponent &skills,
    int char_level
) {
    for (int i = 0; i < 4; ++i) {
        arr.push_back(_build_skill_slot(skills.Slots[i], char_level));
    }
}

} // namespace

void SnapshotBuilder::_build_players(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<
        PlayerTag,
        Position2D,
        FacingAngle,
        Health,
        CombatStats,
        Kills,
        NetworkId,
        Level,
        Experience,
        MoveSpeed>();
    for (auto e : view) {
        auto s = godot::Ref<SimPlayerSnap>(memnew(SimPlayerSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->ang = view.get<FacingAngle>(e).Radians;
        s->hp = view.get<Health>(e).Cur;
        s->max_hp = view.get<Health>(e).Max;
        if (reg.all_of<Mana>(e)) {
            s->mana = reg.get<Mana>(e).Cur;
            s->max_mana = reg.get<Mana>(e).Max;
        }
        s->atk = view.get<CombatStats>(e).Atk;
        s->asp = view.get<CombatStats>(e).Asp;
        s->kills = view.get<Kills>(e).Value;
        s->level = view.get<Level>(e).Value;
        s->xp = view.get<Experience>(e).Cur;
        s->xp_needed = view.get<Experience>(e).Needed;
        s->speed = view.get<MoveSpeed>(e).Value;
        if (reg.all_of<SkillComponent>(e)) {
            _build_skills(
                s->skills, reg.get<SkillComponent>(e), view.get<Level>(e).Value
            );
        }

        // StatusEffect
        s->status = 0;
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            s->status = static_cast<int>(st.Type);
        }

        // CastState fields
        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            s->cast_state = static_cast<int>(cs.State);
            s->cast_slot = cs.ActiveSlot;
            s->cast_aim_x = cs.AimPos.x;
            s->cast_aim_y = cs.AimPos.y;
            s->dash_sx = cs.DashStart.x;
            s->dash_sy = cs.DashStart.y;
            s->dash_tx = cs.DashTarget.x;
            s->dash_ty = cs.DashTarget.y;
            s->hit_target_id = cs.HitTargetId;
            s->cast_error = cs.CastError;

            // Calculate progress
            const auto &def = get_skill_def(cs.SkillId);
            float max_timer = 0.0f;
            if (cs.State == CastState::Phase::Casting)
                max_timer = def.CastTime;
            else if (cs.State == CastState::Phase::Channeling)
                max_timer = def.EffectValue;
            else if (cs.State == CastState::Phase::Dashing)
                max_timer =
                    def.Range /
                    (def.ProjectileSpeed > 0 ? def.ProjectileSpeed : 20.0f);
            s->cast_progress =
                (max_timer > 0.0f) ? (1.0f - cs.Timer / max_timer) : 0.0f;
        }

        snap->players.push_back(s);
    }
}

void SnapshotBuilder::_build_bots(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<
        BotTag,
        Position2D,
        FacingAngle,
        Health,
        CombatStats,
        Kills,
        NetworkId,
        Level,
        Experience,
        MoveSpeed,
        BotTier>();
    for (auto e : view) {
        auto s = godot::Ref<SimBotSnap>(memnew(SimBotSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->ang = view.get<FacingAngle>(e).Radians;
        s->hp = view.get<Health>(e).Cur;
        s->max_hp = view.get<Health>(e).Max;
        s->dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        if (reg.all_of<Mana>(e)) {
            s->mana = reg.get<Mana>(e).Cur;
            s->max_mana = reg.get<Mana>(e).Max;
        }
        s->atk = view.get<CombatStats>(e).Atk;
        s->asp = view.get<CombatStats>(e).Asp;
        s->kills = view.get<Kills>(e).Value;
        s->level = view.get<Level>(e).Value;
        s->xp = view.get<Experience>(e).Cur;
        s->xp_needed = view.get<Experience>(e).Needed;
        s->speed = view.get<MoveSpeed>(e).Value;
        s->tier = static_cast<int>(reg.get<BotTier>(e));
        if (reg.all_of<SkillComponent>(e)) {
            _build_skills(
                s->skills, reg.get<SkillComponent>(e), view.get<Level>(e).Value
            );
        }

        // StatusEffect
        s->status = 0;
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            s->status = static_cast<int>(st.Type);
        }

        snap->bots.push_back(s);
    }
}

void SnapshotBuilder::_build_arrows(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<ArrowTag, Position2D, FacingAngle, NetworkId>();
    for (auto e : view) {
        auto s = godot::Ref<SimArrowSnap>(memnew(SimArrowSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->ang = view.get<FacingAngle>(e).Radians;
        snap->arrows.push_back(s);
    }
}

void SnapshotBuilder::_build_pickups(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<PickupTag, Position2D, NetworkId>();
    for (auto e : view) {
        auto s = godot::Ref<SimPickupSnap>(memnew(SimPickupSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->type = static_cast<int>(view.get<PickupTag>(e).Type);
        s->value = view.get<PickupTag>(e).Value;
        snap->pickups.push_back(s);
    }
}

void SnapshotBuilder::_build_events(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<KillEventBuffer>();
    for (auto e : view) {
        auto &buf = view.get<KillEventBuffer>(e);
        for (auto &ev : buf.events) {
            auto s = godot::Ref<SimEventSnap>(memnew(SimEventSnap));
            s->killer_id = ev.KillerId;
            s->victim_id = ev.VictimId;
            snap->events.push_back(s);
        }
    }
}

void SnapshotBuilder::_build_aoes(
    entt::registry &reg, const godot::Ref<SimSnapshot> &snap
) {
    auto view = reg.view<AoETag, Position2D>();
    for (auto e : view) {
        auto s = godot::Ref<SimAoESnap>(memnew(SimAoESnap));
        auto &aoe = view.get<AoETag>(e);
        auto &pos = view.get<Position2D>(e);
        s->id = reg.all_of<NetworkId>(e) ? reg.get<NetworkId>(e).Value : 0;
        s->x = pos.Value.x;
        s->y = pos.Value.y;
        s->radius = aoe.Radius;
        s->remaining = aoe.Timer;
        s->duration = aoe.Duration;
        snap->aoes.push_back(s);
    }
}

} // namespace sim
