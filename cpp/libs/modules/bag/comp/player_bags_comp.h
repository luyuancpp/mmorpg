#pragma once

#include <array>
#include <unordered_map>

#include "entt/src/entt/entity/registry.hpp"
#include "modules/bag/bag_system.h"

// PlayerBagsComp — ECS component that holds all four Bag instances for
// one player on a scene node. emplace'd on the player entity in
// player_lifecycle.cpp at InitPlayerFromAllData time, removed when
// the entity is DestroyPlayer'd.
//
// Before this component existed (pre-2026-05-17), the Bag class in
// cpp/libs/modules/bag/bag_system.h was constructed only in
// cpp/tests/bag_test — production code had ZERO instantiations.
// That meant cross-zone migration / persistence / rollback couldn't
// carry bag data, because there was no in-memory bag to read from.
//
// Slot layout matches BagType in bag_system.h:
//   bags[kInventory]  = main inventory   (capacity kBagMaxCapacity)
//   bags[kWarehouse]  = warehouse        (capacity kWarehouseMaxCapacity)
//   bags[kEquipment]  = equipment slots  (capacity kEquipmentCapacity)
//   bags[kTemporary]  = temp/loot bag    (capacity kTempBagMaxCapacity)
//
// Capacities themselves are persisted in BagAllData.capacities so that
// gameplay-unlocked slots (Bag::ExpandCapacity) survive across cross-zone hops.
//
// Two storage tiers:
//   * bags          — the fixed, always-present core bags. Dense array
//                     indexed by BagType, cache-friendly fast path.
//   * dynamicBags_  — runtime/temporary bags that come and go (event
//                     bags, per-pet bags). Keyed by a uint64 id (event
//                     id / pet guid). NOT every player has these, and
//                     the set changes at runtime, so they can't live in
//                     the fixed array. Empty for most players.
//
// Both tiers are persisted: cross-zone marshal (bag_marshal.cpp) carries
// the fixed `bags` array via BagAllData.items/capacities and the dynamic
// bags via BagAllData.dynamic_bags.
struct PlayerBagsComp
{
    std::array<Bag, kBagTypeCount> bags{};

    // key = event id / pet guid (whatever owns the transient bag).
    std::unordered_map<uint64_t, Bag> dynamicBags_;
};
