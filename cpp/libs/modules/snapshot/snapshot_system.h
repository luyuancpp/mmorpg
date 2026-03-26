#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "proto/common/rollback/player_snapshot.pb.h"

// Kafka topic for player snapshot entries.
constexpr char kPlayerSnapshotTopic[] = "player_snapshot_topic";

// Current schema version tag — bump when player_database fields change.
constexpr char kSnapshotSchemaVersion[] = "v1";

// Stateless utility that captures a complete serialized snapshot of a player's
// state and sends it to Kafka for persistence.  The Go DB service writes these
// to the `player_snapshot` MySQL table.
//
// Snapshots are the foundation for:
// - Application-level rollback (GM loads a snapshot → diffs → selective restore)
// - Periodic safety nets (timer-driven captures)
// - Pre-trade / pre-maintenance safeguards
//
// Usage:
//   SnapshotSystem::CaptureAndSend(player, SNAPSHOT_LOGIN);
//   SnapshotSystem::CaptureAndSend(player, SNAPSHOT_GM_MANUAL);
class SnapshotSystem
{
public:
    // Marshal current player state, build a PlayerSnapshotEntry, and push to Kafka.
    // Returns the assigned snapshot_id (SnowFlake), or 0 on failure.
    static uint64_t CaptureAndSend(entt::entity player, SnapshotTrigger trigger);
};
