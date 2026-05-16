#pragma once

#include <cstdint>
#include "engine/core/type_define/type_define.h"

namespace muduo
{
    namespace net
    {
        class EventLoop;
    }
}

// CrossZoneReaper — failure-recovery half of the cross-zone migration
// protocol.
//
// Background (see docs/design/cross-zone-readiness-audit.md §3.2 件 3 + §7):
//   The "件 2" pattern (PlayerFrozenComp + delayed DestroyPlayer) makes
//   migration robust against immediate Kafka send failures. But the
//   source-side entity stays frozen until the destination's
//   PlayerMigrationAckEvent arrives. If the broker drops the message,
//   the destination node crashes mid-load, or the source itself restarts
//   between Kafka publish and ACK, the freeze becomes a leak — players
//   stuck offline indefinitely.
//
//   The reaper is the bounded-time recovery for that case.
//
// State store: Redis hash key per in-flight migration:
//   "player_migration:{playerId}"  TTL=60s  fields:
//     from_zone, to_zone, to_node_id, start_ms, attempt
//
//   Written by RecordMigrationStart on `HandleCrossZoneTransfer`.
//   Deleted by RecordMigrationDone on `HandlePlayerMigrationAck`.
//   Surveyed by the reaper Tick to detect stale entries.
//
// Tick policy (every 10s):
//   For each `player_migration:*` key whose from_zone == this zone AND
//   start_ms older than per-attempt deadline:
//     1. attempt < 3 → re-publish player_migrate Kafka event, attempt++
//     2. attempt == 3 → declare migration failed: remove
//        PlayerFrozenComp, send "transfer aborted" tip to client, delete
//        Redis key.
//
// Restart-recovery policy:
//   On scene-node startup (after Initialize but before accepting client
//   traffic), call ScanAndRecover. Every player_migration:* key with
//   from_zone == self is one of:
//     a) the destination ACK'd while we were offline → entity already
//        gone in our local registry; just delete the Redis key.
//     b) we never got ACK and the entity is gone too (we restarted past
//        a Frozen player) → also delete the Redis key.
//     c) edge case where the entity is still in our local registry
//        somehow → treat as fresh in-flight, re-arm timer.
//   Distinguishing a/b is not necessary — both call for "delete the
//   stale Redis key, log it, move on".
class CrossZoneReaper
{
public:
    // Per-attempt deadline before reaper re-publishes (case 1) or
    // declares failure (case 2). 30s aligns with the player_locator
    // reconnect lease (mmo_cross_server_architecture.md §8 Layer 1).
    static constexpr int64_t kPerAttemptDeadlineMs = 30 * 1000;

    // How often the reaper scans Redis for stale migrations.
    static constexpr double kTickIntervalSec = 10.0;

    // Maximum re-publish attempts before declaring migration failed.
    // Includes the original publish from HandleCrossZoneTransfer, so
    // attempt=1 is "first try in flight", attempt=3 is "two retries
    // failed, this is the last chance".
    static constexpr uint32_t kMaxAttempts = 3;

    // Attach the reaper's periodic tick to the scene event loop. Idempotent
    // — calling twice replaces the existing timer. Call from
    // SetAfterStart in scene/main.cpp after Redis is connected.
    static void StartTick(muduo::net::EventLoop *loop);

    // Cancel the periodic tick. Safe to call before StartTick (no-op).
    static void StopTick();

    // Persist a migration-in-flight record. Called from
    // HandleCrossZoneTransfer right after the Kafka send succeeds.
    //
    // attempt=1 for the original send; the reaper bumps it on retries.
    static void RecordMigrationStart(Guid playerId, uint32_t fromZone,
                                     uint32_t toZone, uint32_t toNodeId,
                                     uint32_t attempt);

    // Delete the migration record on successful ACK. Called from
    // HandlePlayerMigrationAck before DestroyPlayer.
    //
    // Tolerates missing records (DEL is idempotent in Redis) — duplicate
    // ACKs from Kafka rebalance won't error.
    static void RecordMigrationDone(Guid playerId);

    // Scan-and-recover sweep, run once at scene-node startup.
    // Removes all `player_migration:*` keys this node owns whose
    // matching local entity is no longer present (case a/b above).
    static void ScanAndRecover();
};
