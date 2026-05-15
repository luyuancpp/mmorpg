#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "proto/common/database/player_cache.pb.h"

// Last-persisted snapshot for proto-compare dirty-save (todo.md #204 / #226).
//
// This component is attached to a player entity AFTER a successful
// HandlePlayerAsyncSaved and holds the exact bytes that were just
// committed to Redis. On the next SavePlayerToRedis, the new
// PlayerAllData is compared (via dirty_save::IsEqual) against this
// snapshot; if equal, the save is skipped — Redis write + Kafka
// DBTask emission both avoided.
//
// Lifetime:
//   - Created on the FIRST successful save for a player. Before that,
//     no component exists → ShouldPersist returns true unconditionally
//     (the dirty_save::ShouldPersist helper handles nullptr).
//   - Replaced wholesale on every subsequent successful save.
//   - Cleared when the player entity is destroyed (entt removes
//     components with the entity, no manual cleanup needed).
//
// Memory cost:
//   - One `PlayerAllData` per online player. For a typical player snapshot
//     of ~5 KB and a 10k-online-player gate, ~50 MB of additional
//     memory. Acceptable for a modern dedicated game server; if a
//     deployment finds it too much, the slice B integration can swap
//     to a hash-only model and accept hash-collision false negatives.
//
// Why a unique_ptr instead of holding the proto directly:
//   - PlayerAllData is generated protobuf with non-trivial constructors.
//     Holding it by pointer keeps the component cheap to default-
//     construct (the registry creates one on entt::emplace before we
//     fill it in) and lets us pay the proto allocation cost only when
//     we actually have a snapshot to record.
//   - std::unique_ptr means the component owns the proto exclusively;
//     no shared-mutation hazards across systems.
//   - Replacement is a single pointer swap, not a deep copy from the
//     SavePlayerToRedis hot path.
struct PlayerLastPersistedSnapshotComp
{
    // The persisted proto; nullptr until the first save completes.
    std::unique_ptr<PlayerAllData> snapshot;

    // Wall-clock millis when `snapshot` was set. Useful for forensics
    // ("how stale is our last-persisted state?") and for telemetry on
    // save-skip cadence. 0 when snapshot is nullptr.
    int64_t saved_at_ms{0};

    // Helper: replace the snapshot in one call. Allocates a new
    // PlayerAllData copy and stamps the timestamp. Cheap to call
    // from HandlePlayerAsyncSaved.
    void Replace(const PlayerAllData& fresh)
    {
        if (!snapshot)
        {
            snapshot = std::make_unique<PlayerAllData>(fresh);
        }
        else
        {
            *snapshot = fresh;
        }
        saved_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // True iff a snapshot has been recorded (first save completed).
    bool HasSnapshot() const { return snapshot != nullptr; }
};
