#pragma once

#include <cstdint>

// PlayerFrozenComp — cross-zone migration in-flight marker.
//
// Set on the source-side player entity AFTER `HandleCrossZoneTransfer` has
// published the `player_migrate` Kafka event but BEFORE `DestroyPlayer` runs.
// Replaces the old "Kafka send → immediate DestroyPlayer" pattern, which
// silently lost players on Kafka broker / destination-node failure.
//
// While this component is present:
//   • AOI / combat / movement / chat / business systems MUST skip the entity.
//   • Currency Add/Deduct MUST reject (treat as already-gone for write
//     purposes — equivalent to "non-existent" so Single Writer holds).
//   • Gate session is retained so the client stays connected; if the migration
//     fails the source can unfreeze and continue serving the player without
//     a forced reconnect.
//
// Cleared (and DestroyPlayer fires) when:
//   • The source receives the matching `player_migrate_ack` from the
//     destination zone (the success path).
//   • The reaper declares the migration failed after N retries; the source
//     unfreezes the entity and notifies the client (the failure path).
//
// This is a pure runtime marker — NO persistence, NO serialization, NO IPC.
// Proto is the wrong tool for "transient ECS tag", so this lives as a plain
// C++ struct in scene's local module.
//
// See docs/design/cross-zone-readiness-audit.md §3.2 件 2 for the full
// design rationale and §7 for the failure scenarios that motivated it.
struct PlayerFrozenComp
{
    // Wall-clock millis when the freeze started — matches when the
    // `player_migrate` event was published. Used by the reaper to detect
    // ACK timeouts.
    int64_t frozenAtMs{0};

    // Destination zone the player is migrating to. Used by metrics and
    // by the reaper when generating retry payloads.
    uint32_t toZoneId{0};

    // Number of `player_migrate` send attempts made so far. Incremented
    // by the reaper on retry. Bounded by the migration policy (default 3
    // — see cross-zone-readiness-audit.md §7 失败 A).
    uint32_t migrateAttempts{1};
};
