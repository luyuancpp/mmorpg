#pragma once

#include <array>

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
// Slot layout matches EnumBagType in bag_system.h:
//   bags[kBag]        = main inventory   (capacity kBagMaxCapacity)
//   bags[kWarehouse]  = warehouse        (capacity kWarehouseMaxCapacity)
//   bags[kEquipment]  = equipment slots  (capacity kEquipmentCapacity)
//   bags[kTemporary]  = temp/loot bag    (capacity kTempBagMaxCapacity)
//
// Capacities themselves are persisted in BagAllData.capacities so that
// gameplay-unlocked slots (Bag::Unlock) survive across cross-zone hops.
struct PlayerBagsComp
{
    std::array<Bag, kBagMax> bags{};
};
