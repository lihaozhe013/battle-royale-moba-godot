#pragma once

#include "snapshot_types.h"
#include <entt/entt.hpp>

namespace sim {

class SnapshotBuilder {
  public:
    static godot::Ref<SimSnapshot> build(entt::registry &reg, int seq);

  private:
    static void
    _build_players(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
    static void
    _build_bots(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
    static void
    _build_arrows(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
    static void
    _build_pickups(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
    static void
    _build_events(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
    static void
    _build_aoes(entt::registry &reg, const godot::Ref<SimSnapshot> &snap);
};

} // namespace sim
