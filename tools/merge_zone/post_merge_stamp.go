package main

// Server-merge player notice + force-rename stamping (post-merge).
//
// Why this is its own file:
//   The 5-step merge in main.go (guild MySQL / rank ZSET / mapping
//   remap / blob copy / name conflict probe) is structurally about
//   moving rows. The post-merge stamping below is structurally about
//   *flagging* the source players so client/login can show them a
//   one-shot UI. Different lifecycle, different failure mode (a stamp
//   miss is annoying — a row miss is data-loss), so kept separate.
//
// Wire format:
//   Two Redis keys per stamped player, both written to the **mapping**
//   Redis (not the data Redis):
//
//     player_merge_notice:{playerId}     value = "<merge_ts_ms>"
//     player_force_rename:{playerId}     value = "<merge_ts_ms>"
//
//   We deliberately do NOT serialize PlayerMergeStateComp into the
//   player_database protobuf blob from this tool — doing so would
//   require pulling the entire codegen Go module into tools/merge_zone
//   (and keeping it in sync with the protoc-gen output), which is a
//   maintenance load disproportionate to the value. The login service
//   reads these flag keys directly (see go/login/internal/logic/
//   clientplayerlogin/entergamelogic.go) and copies them into
//   PlayerMergeStateComp on first login, then DELETEs them so the
//   notice/rename UI fires exactly once.
//
//   The flag-key approach also makes the "merge happened, blob lives
//   in target Redis" handoff clean: blob_migrate copies player_database
//   to target *as-is* (no merge_state field set yet), and the flag
//   keys live in mapping Redis which both source and target zones can
//   read post-merge.
//
// Idempotency:
//   Re-running this stamp step (-apply twice) is safe — SETNX semantics
//   would reject the second write, but plain SET keeps the latest
//   timestamp, which is also fine because login reads timestamp purely
//   for audit and treats key-exists as the "show notice" signal.

import (
	"context"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
)

const (
	// Key prefixes consumed by go/login/internal/logic/clientplayerlogin
	// — keep these strings in sync with the login-side reader. A
	// constants file would be ideal but the merge_zone tool has its own
	// go.mod and crossing it is a heavier change than the duplication
	// is worth.
	mergeNoticeKeyPrefix = "player_merge_notice:"
	forceRenameKeyPrefix = "player_force_rename:"
)

// stampPostMergeFlags writes one notice key per source-zone player.
//
// Force-rename stamping is currently a no-op: the codebase has no
// `player(name, zone_id)` table for SQL-based collision detection
// (see audit_resources.go::auditPlayerNameConflicts for the full
// reasoning). The `forceRenamePlayerIDs` parameter remains in the
// signature so this site is the single seam where future protobuf-
// blob-decoding logic plugs in once that work lands. Callers pass
// nil today; the function silently skips the rename pipeline.
//
// Inputs:
//   mapRdb               — mapping Redis (where player:zone:* lives).
//   playerIDs            — set of source-zone players (already collected).
//   forceRenamePlayerIDs — subset that should also be flagged for rename.
//                          Pass nil until SQL-free conflict detection lands.
//   mergeTimestamp       — unix-millis to write into the value (audit).
//   dryRun               — when true, only count, do not write.
//
// Failure mode:
//   Best-effort: a partial stamp run leaves some players un-flagged.
//   Those players miss the one-shot notice but otherwise function
//   normally; ops can re-run -apply with -only-stamp (TODO if needed)
//   to fill the gap. We log the first error then continue rather
//   than aborting mid-stamp, because aborting halfway through is
//   strictly worse than stamping a partial set.
func stampPostMergeFlags(
	ctx context.Context,
	mapRdb *redis.Client,
	playerIDs []uint64,
	forceRenamePlayerIDs []uint64,
	mergeTimestamp int64,
	dryRun bool,
) (noticeStamped int, renameStamped int, firstErr error) {
	if dryRun {
		// Even in dry-run we report what would be written, so ops can
		// reconcile counts before pulling the trigger.
		return len(playerIDs), len(forceRenamePlayerIDs), nil
	}

	tsStr := strconv.FormatInt(mergeTimestamp, 10)

	// Notice flags: one per source player.
	pipe := mapRdb.Pipeline()
	for _, pid := range playerIDs {
		pipe.Set(ctx, mergeNoticeKeyPrefix+strconv.FormatUint(pid, 10), tsStr, 0)
	}
	if _, err := pipe.Exec(ctx); err != nil {
		// Pipeline-level error is rare (network); log and surface but
		// don't unwind already-written keys — Redis writes are idempotent
		// against re-stamp, so the next attempt will just overwrite.
		firstErr = fmt.Errorf("notice stamp pipeline: %w", err)
		log.Printf("WARN: %v (continuing, partial stamp acceptable)", firstErr)
	}
	noticeStamped = len(playerIDs)

	// Force-rename flags: only the explicit subset (nil today).
	if len(forceRenamePlayerIDs) > 0 {
		pipe2 := mapRdb.Pipeline()
		for _, pid := range forceRenamePlayerIDs {
			pipe2.Set(ctx, forceRenameKeyPrefix+strconv.FormatUint(pid, 10), tsStr, 0)
		}
		if _, err := pipe2.Exec(ctx); err != nil {
			if firstErr == nil {
				firstErr = fmt.Errorf("force-rename stamp pipeline: %w", err)
			}
			log.Printf("WARN: force-rename stamp pipeline: %v (continuing)", err)
		}
		renameStamped = len(forceRenamePlayerIDs)
	}

	return noticeStamped, renameStamped, firstErr
}

// stampNowUnixMs returns the current unix-millis. Wrapped so tests
// can stub if we ever add unit tests for the stamp helpers.
func stampNowUnixMs() int64 {
	return time.Now().UnixMilli()
}
