#pragma once

// BagMarshal — bridge between BagAllData proto (cross-zone payload /
// persistence blob) and the player's runtime bag state.
//
// Status (2026-05-16):
//   The Bag class in cpp/libs/modules/bag/bag_system.h is NOT currently
//   attached to player entities in production code — `grep -rn
//   "emplace<Bag>"` returns zero hits outside cpp/tests/. So the
//   Marshal/Unmarshal functions below are stubs that handle the proto
//   side correctly but have nothing on the ECS side to read/write.
//
//   The proto wire path is plumbed (PlayerAllData carries BagAllData in
//   field 4), so cross-zone migration can ship without bag data drop —
//   it just won't carry items until bag-to-entity attach lands.
//
//   When bag attaches to player entity (track via task #21 and the bag
//   integration design discussion that hasn't started yet), update both
//   functions to walk the bag's items_ map and emit ItemEntry per
//   {bag_type=0..3, pos, item_uuid, config_id, stack_size}. The proto
//   schema (BagAllData) and ItemEntry layout already accommodate this —
//   see proto/common/database/bag_quest_mail_data.proto for game-design
//   field TODOs at known field numbers.

#include "entt/src/entt/entity/registry.hpp"
#include "proto/common/database/bag_quest_mail_data.pb.h"

namespace bag_marshal
{
    // Read all bag/warehouse/equipment/temp items off the player's ECS
    // representation, write them to `out`. Idempotent — calling twice
    // produces equivalent BagAllData (modulo serialization order, which
    // proto3 doesn't guarantee anyway).
    //
    // CURRENT BEHAVIOR (2026-05-16): no-op. Bag isn't attached to player
    // entity in production code. The function exists so PlayerAllData
    // marshalling has a stable ABI for the next iteration.
    void Marshal(entt::entity player, BagAllData& out);

    // Reverse direction: rebuild bag/warehouse/equipment/temp ECS state
    // from the BagAllData payload. Called on cross-zone destination
    // when receiving a migrating player, on player login when loading
    // from Redis, and on rollback when restoring a snapshot.
    //
    // CURRENT BEHAVIOR (2026-05-16): no-op for the same reason as Marshal.
    void Unmarshal(entt::entity player, const BagAllData& in);
}
