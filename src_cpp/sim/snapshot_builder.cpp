#include "snapshot_builder.h"
#include "components.h"
#include "game_config.h"
#include <chrono>

namespace sim {

godot::Ref<SimSnapshot> SnapshotBuilder::build(entt::registry &reg, int seq) {
    auto snap = godot::Ref<SimSnapshot>(memnew(SimSnapshot));
    snap->seq = seq;
    snap->t = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    _build_players(reg, snap);
    _build_bots(reg, snap);
    _build_arrows(reg, snap);
    _build_pickups(reg, snap);
    _build_events(reg, snap);

    return snap;
}

void SnapshotBuilder::_build_players(entt::registry &reg, const godot::Ref<SimSnapshot> &snap) {
    auto view = reg.view<PlayerTag, Position2D, FacingAngle, Health, CombatStats,
                          Kills, NetworkId, Level, Experience, MoveSpeed>();
    for (auto e : view) {
        auto s = godot::Ref<SimPlayerSnap>(memnew(SimPlayerSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->ang = view.get<FacingAngle>(e).Radians;
        s->hp = view.get<Health>(e).Cur;
        s->max_hp = view.get<Health>(e).Max;
        s->atk = view.get<CombatStats>(e).Atk;
        s->asp = view.get<CombatStats>(e).Asp;
        s->kills = view.get<Kills>(e).Value;
        s->level = view.get<Level>(e).Value;
        s->xp = view.get<Experience>(e).Cur;
        s->xp_needed = view.get<Experience>(e).Needed;
        s->speed = view.get<MoveSpeed>(e).Value;
        snap->players.push_back(s);
    }
}

void SnapshotBuilder::_build_bots(entt::registry &reg, const godot::Ref<SimSnapshot> &snap) {
    auto view = reg.view<BotTag, Position2D, FacingAngle, Health, CombatStats, Kills,
                          NetworkId, Level>();
    for (auto e : view) {
        auto s = godot::Ref<SimBotSnap>(memnew(SimBotSnap));
        s->id = view.get<NetworkId>(e).Value;
        s->x = view.get<Position2D>(e).Value.x;
        s->y = view.get<Position2D>(e).Value.y;
        s->ang = view.get<FacingAngle>(e).Radians;
        s->hp = view.get<Health>(e).Cur;
        s->max_hp = view.get<Health>(e).Max;
        s->dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        s->atk = view.get<CombatStats>(e).Atk;
        s->asp = view.get<CombatStats>(e).Asp;
        s->kills = view.get<Kills>(e).Value;
        s->level = view.get<Level>(e).Value;
        snap->bots.push_back(s);
    }
}

void SnapshotBuilder::_build_arrows(entt::registry &reg, const godot::Ref<SimSnapshot> &snap) {
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

void SnapshotBuilder::_build_pickups(entt::registry &reg, const godot::Ref<SimSnapshot> &snap) {
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

void SnapshotBuilder::_build_events(entt::registry &reg, const godot::Ref<SimSnapshot> &snap) {
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

} // namespace sim
