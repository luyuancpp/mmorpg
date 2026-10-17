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
    //
    // 拷贝之后清掉 last_save_seq(docs/design/db-write-fencing-mysql.md §7.3):它是每次写盘都变的存盘序号,
    // 不是业务数据。快路径拿「新 marshal 的消息」与本快照比 dirty_save::IsEqual,新消息在比较时还没发号
    // (SavePlayerToRedisImpl 在快路径之后才写 last_save_seq),快照若带着序号,快路径永远不命中。
    // 剥离只在落地时做一次,不在每次存盘尝试的热路径上拷贝。快照的其他读者(资产 / 结算 / 收件箱账本的
    // 「已落盘」判定)都不读这个字段。
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
        snapshot->clear_last_save_seq();
        saved_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // True iff a snapshot has been recorded (first save completed).
    bool HasSnapshot() const { return snapshot != nullptr; }
};
