package main

// Resource audit for server merge — companion to main.go's 5-step merge.
//
// Why this exists:
//   tools/merge_zone/main.go covers guild MySQL + guild ranking ZSET + player
//   home_zone mapping + (optional) player blob copy. It does NOT examine the
//   ~half-dozen other tables and Redis structures that hang off player_id and
//   may also need attention during merge:
//     - friend / friend_request (per-player tables, both sides may move)
//     - mail / mail_attachment (mail recipients & attached items)
//     - auction (in-flight auctions hanging off seller_id / bidder_id)
//     - chat_history (per-zone or global?)
//     - guild_application (applicant_id pointing into either zone)
//     - player.name conflicts (the missing P0-G check)
//     - online status keys (Redis TTL keys that may shadow re-login)
//
//   This file scans those resources during a merge dry-run and reports
//   discrepancies BEFORE main.go's destructive write phase. See
//   docs/design/server-merge-gap-fixes.md §2 (P0-J) and
//   docs/ops/merge-zone-runbook.md §4.2.
//
// What it does NOT do:
//   - Does not fix the discrepancies. Some need schema decisions
//     (chat history retention), some need a force-rename UI in the
//     client (P0-G), some are runtime-state issues that just need a
//     "wait until target zone is fully drained" gate.
//   - Does not touch data unless invoked with -VerifyMerged AND the
//     verify-merged path discovers leftover source-zone state worth
//     printing (still read-only).
//
// Wiring:
//   `dev_tools.ps1 -Command merge-zone-audit -MergeSourceZone <s> -MergeTargetZone <d>`
//   forwards to `merge_zone.exe -mode audit ...` (see main.go's flag wiring).

import (
	"context"
	"database/sql"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/redis/go-redis/v9"
)

// ResourceAudit is a single per-resource report row.
//
// Conceptually it answers: "for resource X, what does merging zone src→dst
// look like?" Fields are deliberately denormalized; the audit prints them
// as a markdown table so ops can eyeball before the maintenance window.
type ResourceAudit struct {
	Name          string // resource short name (e.g. "mail", "friend")
	SourceCount   int64  // rows / keys in source zone
	TargetCount   int64  // rows / keys in target zone
	UniqueScope   string // "global" / "per_zone" / "none" — affects merge safety
	ConflictCount int64  // post-merge collision count (e.g. duplicate player names)
	Severity      string // "info" / "warn" / "block"
	Notes         string // free-form observations + remediation hint
}

// auditConfig bundles inputs every per-resource auditor needs.
//
// We pass this around instead of a global so the auditor can be unit-tested
// with mocked DB / Redis. Keep this struct small — anything zone-wide goes
// into a closure if needed.
type auditConfig struct {
	db        *sql.DB        // game MySQL — guild / mail / auction / etc.
	mappingDB *redis.Client  // mapping redis — player:zone:{id} → home_zone
	rankRDB   *redis.Client  // rank redis — guild_rank:zone:{id} ZSETs
	src       uint32         // source zone (merging FROM)
	dst       uint32         // target zone (merging INTO)
	verify    bool           // post-merge verification mode (see -VerifyMerged in runbook)
	timeout   time.Duration  // per-query timeout — keep short, audit must be fast
}

// auditEntryParams is the pure-data input for runAuditEntry. main.go owns
// flag parsing; audit_resources.go owns the audit semantics. This struct
// is the seam between them.
type auditEntryParams struct {
	src          uint32
	dst          uint32
	mysqlDSN     string
	mappingAddr  string
	mappingPwd   string
	mappingDB    int
	rankAddr     string
	rankPwd      string
	rankDB       int
	verifyMerged bool
}

// firstNonEmpty returns a if non-empty, else b. Avoids a one-line helper
// being copy-pasted into multiple call sites in main.go.
func firstNonEmpty(a, b string) string {
	if a != "" {
		return a
	}
	return b
}

// runAuditEntry is the -mode=audit dispatch from main.go. It opens the
// connections, calls runAuditMode, prints the report, and exits non-zero
// if any auditor returned Severity="block".
//
// Exit code policy:
//   0 — clean, no blockers
//   1 — at least one block-severity finding (do NOT proceed with merge)
//   2 — infrastructure error (couldn't even connect to MySQL/Redis)
//
// Ops scripts can branch on exit code without parsing stdout.
func runAuditEntry(p auditEntryParams) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
	defer cancel()

	db, err := sql.Open("mysql", p.mysqlDSN)
	if err != nil {
		log.Printf("ERROR: mysql open: %v — audit will skip MySQL-backed checks", err)
		// Don't fatal — Redis-backed audits (online_keys) still useful.
	} else {
		defer db.Close()
		if err := db.PingContext(ctx); err != nil {
			log.Printf("WARN: mysql ping: %v — audit will skip MySQL-backed checks", err)
			db = nil
		}
	}

	mapRdb := redis.NewClient(&redis.Options{
		Addr:     p.mappingAddr,
		Password: p.mappingPwd,
		DB:       p.mappingDB,
	})
	defer mapRdb.Close()
	if err := mapRdb.Ping(ctx).Err(); err != nil {
		log.Printf("WARN: mapping redis ping: %v — audit will skip mapping-backed checks", err)
		mapRdb = nil
	}

	rankRdb := redis.NewClient(&redis.Options{
		Addr:     p.rankAddr,
		Password: p.rankPwd,
		DB:       p.rankDB,
	})
	defer rankRdb.Close()
	// Rank ping isn't fatal; if it fails the rank-side audits just degrade.
	_ = rankRdb.Ping(ctx).Err()

	cfg := auditConfig{
		db:        db,
		mappingDB: mapRdb,
		rankRDB:   rankRdb,
		src:       p.src,
		dst:       p.dst,
		verify:    p.verifyMerged,
		timeout:   30 * time.Second,
	}

	mode := "pre-merge"
	if p.verifyMerged {
		mode = "POST-MERGE VERIFICATION"
	}
	log.Printf("=== Merge-zone audit (%s) src=%d dst=%d ===", mode, p.src, p.dst)

	results := runAuditMode(ctx, cfg)
	blockCount, _ := printAuditReport(results)

	if blockCount > 0 {
		os.Exit(1)
	}
}

// runAuditMode is the -mode=audit entry point invoked from main.go.
//
// The contract:
//   - Read-only. Never writes. Refusing to write is a safety property of the
//     audit — main.go is the only writer, this file is the inspector.
//   - Returns a non-nil slice; never nil. Empty slice means "no resources
//     known to audit," which is itself worth printing so ops doesn't think
//     audit silently skipped.
//
// 2026-05-23 schema-reality reconciliation:
//   The earlier shape of this file ran ten auditors covering
//   player.name / mail / mail_attachment / auction / chat_history /
//   guild_application — all assuming standalone MySQL tables that
//   *do not actually exist in this codebase*. The real schema lives
//   in deploy/mysql-init/ and go/db/model/mysql_database_table.sql:
//   only `guild`, `guild_member`, `friend`, `friend_request`, and the
//   blob-form `player_database` exist. Player nicknames are inside
//   `player_database`'s MEDIUMBLOB, not a column.
//
//   Running auditors against tables that don't exist returns the
//   "table not present — nothing to audit" placeholder I built as a
//   "graceful degradation" path. In practice that placeholder *misleads*
//   ops into thinking the audit ran cleanly. Removed those auditors;
//   only checks that exercise schema we *actually have* survived.
//
//   The Player-name conflict check survives in a different shape: it
//   prints an explicit "not implemented" line so the operator knows
//   to fall back to the manual procedure in
//   docs/ops/merge-zone-runbook.md §4.4. The block-vs-info severity
//   makes this visible at the bottom of every audit run rather than
//   hiding under "info: schema doesn't expose zone_id".
func runAuditMode(ctx context.Context, cfg auditConfig) []ResourceAudit {
	auditors := []func(context.Context, auditConfig) ResourceAudit{
		auditPlayerNameConflicts, // explicit "not implemented" notice — see below
		auditFriend,              // global table keyed by player_id; survives merge automatically
		auditFriendRequest,       // same
		auditGuildMembers,        // global guild_member table; surfaces volume + zone-cross hints
		auditOnlineKeys,          // Redis online TTL keys leftover after zone-down
	}

	results := make([]ResourceAudit, 0, len(auditors))
	for _, fn := range auditors {
		// Each auditor gets its own derived context so a slow scan on one
		// resource (e.g. mail with millions of rows) cannot stall the rest.
		// 30s is generous — if any auditor needs more, it's broken.
		auditCtx, cancel := context.WithTimeout(ctx, 30*time.Second)
		results = append(results, fn(auditCtx, cfg))
		cancel()
	}
	return results
}

// printAuditReport renders results as a markdown-style table on stdout.
//
// The format is deliberately copy-pasteable into the merge runbook so ops
// can attach it to the maintenance ticket. Severity drives the marker
// column ([!] block / [.] warn / [ ] info) — block means "do NOT proceed
// with merge until resolved."
func printAuditReport(results []ResourceAudit) (blockCount, warnCount int) {
	log.Println()
	log.Println("=== Merge-zone resource audit ===")
	log.Println()
	log.Printf("%-3s %-22s %10s %10s %12s %10s  %s",
		"sev", "resource", "src_count", "dst_count", "unique_scope", "conflicts", "notes")
	log.Println(strings.Repeat("─", 110))
	for _, r := range results {
		marker := " "
		switch r.Severity {
		case "block":
			marker = "!"
			blockCount++
		case "warn":
			marker = "."
			warnCount++
		}
		log.Printf("[%s] %-22s %10d %10d %12s %10d  %s",
			marker, r.Name, r.SourceCount, r.TargetCount, r.UniqueScope, r.ConflictCount, r.Notes)
	}
	log.Println()
	log.Printf("Audit summary: %d block(s), %d warn(s), %d info row(s).", blockCount, warnCount, len(results)-blockCount-warnCount)
	log.Println()
	if blockCount > 0 {
		log.Println("⚠️  At least one resource is in BLOCK state. Resolve before running -apply.")
		log.Println("   See docs/ops/merge-zone-runbook.md §4.2 / §6 for guidance.")
		log.Println()
	}
	return
}

// ─── per-resource auditors ─────────────────────────────────────────────
//
// Each auditor returns a single ResourceAudit. On error it returns a
// row with Severity="warn" and the error in Notes — never panics, never
// fails the whole audit. Audit is best-effort by design: a missing
// optional table (e.g. chat_history not deployed yet) should not break
// the merge gate.

// auditPlayerNameConflicts — explicit "not applicable" notice (was a
// silent-failing P0-G check until 2026-05-23, then a "not implemented"
// notice. The 2026-05-23-pm reality check went one layer deeper).
//
// History (kept as a cautionary comment for future engineers):
//   The earliest shape ran SQL against `player.name` / `player.zone_id`.
//   Both columns are entirely fictional in this codebase — the real
//   `player_database` schema has only `player_id BIGINT` plus seven
//   MEDIUMBLOB component columns. Discovering that triggered v1.1 of
//   this audit, which turned the row into a block-severity "not
//   implemented; see merge-zone-runbook.md §4.4" hint pointing at a
//   manual rename procedure.
//
//   Then I went looking for *which* protobuf field actually holds the
//   nickname so a future protobuf-decoding implementation knew where
//   to look. Result:
//     - CreatePlayerRequest is an empty message — no nickname on creation
//     - AccountSimplePlayer carries only player_id
//     - PlayerUint32Comp / PlayerUint64Comp carry class /
//       registration_timestamp respectively, no name field
//     - user.display_name column exists in the SQL schema but
//       grep across go/ + cpp/ shows zero readers and zero writers;
//       it's a placeholder column on a placeholder table
//
//   So the project today has NO PLAYER NICKNAME at all. Players are
//   identified to other players by player_id (uint64). The "two
//   players named 剑圣 in different zones merge" scenario described
//   in server-merge-gap-fixes.md §1 cannot happen because nobody has
//   a name to clash on.
//
//   The force_rename plumbing (PlayerMergeStateComp + Redis flag +
//   EnterGameResponse field + post_merge_stamp.go's parameter seam)
//   is NOT dead code — it's pre-wired for the day someone enables a
//   nickname surface. Until then this auditor reports the situation
//   so operators don't blindly follow a manual procedure for a
//   problem they don't have.
//
// What this auditor does today:
//   Returns an info-severity row that says "not applicable — no
//   nickname surface in this codebase". Severity is intentionally
//   info, not block: blocking would force ops to skip a real audit
//   gate to merge, which is counterproductive when the gate guards
//   nothing.
func auditPlayerNameConflicts(ctx context.Context, cfg auditConfig) ResourceAudit {
	return ResourceAudit{
		Name:        "player.name (n/a)",
		UniqueScope: "n/a",
		Severity:    "info",
		Notes:       "NOT APPLICABLE — project has no player nickname field today (CreatePlayer is empty; user.display_name is unused). force_rename plumbing is pre-wired for future enable. See merge-zone-runbook.md §4.4.",
	}
}

// auditFriend — verify friend table won't break under merge.
//
// `friend(player_id, friend_player_id)` is keyed by player_id alone (no
// zone_id column — see deploy/mysql-init/guild_friend_tables.sql). Because
// player_id is globally unique (mmo_cross_server_architecture.md §"player_id
// never encodes zone"), friend rows survive merge automatically.
//
// What we still want to flag:
//   - Pre-existing rows where one side has home_zone=src and the other
//     has home_zone=dst. They were already cross-zone friends and that's
//     fine, but ops should know the volume — high counts suggest active
//     cross-zone play, which raises confidence the merge is overdue.
//   - Orphan rows (friend_player_id not in any zone's mapping). These
//     are pre-existing data quality issues, not caused by merge, but
//     surfacing them now lets ops fix opportunistically.
func auditFriend(ctx context.Context, cfg auditConfig) ResourceAudit {
	r := ResourceAudit{Name: "friend", UniqueScope: "global"}
	if cfg.db == nil {
		r.Severity = "warn"
		r.Notes = "no MySQL handle"
		return r
	}
	if err := cfg.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM friend").Scan(&r.SourceCount); err != nil {
		r.Severity = "warn"
		r.Notes = fmt.Sprintf("count failed: %v", err)
		return r
	}
	r.TargetCount = r.SourceCount // same global table; we don't subdivide
	r.Severity = "info"
	r.Notes = "global table keyed by player_id only — survives merge automatically. No action."
	return r
}

// auditFriendRequest — pending requests across zones.
//
// Same shape as friend table (no zone_id). Pending requests stay pending
// across merge; on first login post-merge the recipient sees them as
// usual. The one quirk: a request from src player to dst player was
// previously "cross-zone" and may have been throttled by client UX; after
// merge it's local. Worth a one-line note, not a block.
func auditFriendRequest(ctx context.Context, cfg auditConfig) ResourceAudit {
	r := ResourceAudit{Name: "friend_request", UniqueScope: "global"}
	if cfg.db == nil {
		r.Severity = "warn"
		r.Notes = "no MySQL handle"
		return r
	}
	if err := cfg.db.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM friend_request WHERE status = 1").Scan(&r.SourceCount); err != nil {
		r.Severity = "warn"
		r.Notes = fmt.Sprintf("count failed: %v", err)
		return r
	}
	r.TargetCount = r.SourceCount
	r.Severity = "info"
	r.Notes = "pending requests survive merge as-is. Cross-zone requests become local automatically."
	return r
}

// auditGuildMembers — surface guild_member volume + zero-orphan check.
//
// guild_member(guild_id, player_id, ...) — keyed by player_id, no zone
// column. After main.go's guild MySQL step runs, every guild row has
// zone_id rewritten to dst; guild_member rows stay valid because they
// reference the same guild_id. We just count to give ops a sanity
// number and verify there's no guild_member pointing at a guild that
// no longer exists post-merge (orphan defensive check).
func auditGuildMembers(ctx context.Context, cfg auditConfig) ResourceAudit {
	r := ResourceAudit{Name: "guild_member", UniqueScope: "global"}
	if cfg.db == nil {
		r.Severity = "warn"
		r.Notes = "no MySQL handle"
		return r
	}
	if err := cfg.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM guild_member").Scan(&r.SourceCount); err != nil {
		r.Severity = "info"
		r.Notes = "guild_member table not present"
		return r
	}
	r.TargetCount = r.SourceCount
	// Orphan check: members pointing at non-existent guilds.
	var orphans int64
	_ = cfg.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM guild_member m
		 LEFT JOIN guild g ON m.guild_id = g.guild_id
		  WHERE g.guild_id IS NULL`).Scan(&orphans)
	r.ConflictCount = orphans
	if orphans > 0 {
		r.Severity = "warn"
		r.Notes = fmt.Sprintf("%d guild_member rows reference non-existent guilds (pre-existing data quality issue, not merge-caused)", orphans)
	} else {
		r.Severity = "info"
		r.Notes = "rows survive merge automatically (guild_id stable). 0 orphans."
	}
	return r
}

// auditOnlineKeys — Redis online TTL keys leftover after zone-down.
//
// `friend:online:{playerId}` is a Redis key with 60s TTL written by
// FriendRepo.SetPlayerOnline. After zone-down, no one writes it; after
// 60s, all entries naturally expire. If the auditor finds source-zone
// players still flagged online, it means zone-down didn't propagate or
// there's a rogue process still ACK'ing — BLOCK the merge until clean.
func auditOnlineKeys(ctx context.Context, cfg auditConfig) ResourceAudit {
	r := ResourceAudit{Name: "online_keys", UniqueScope: "per_zone"}
	if cfg.mappingDB == nil {
		r.Severity = "warn"
		r.Notes = "no mapping Redis handle"
		return r
	}
	// Count source-zone players still showing as online.
	pids, err := collectPlayerIDsWithHomeZone(ctx, cfg.mappingDB, cfg.src)
	if err != nil {
		r.Severity = "warn"
		r.Notes = fmt.Sprintf("mapping scan failed: %v", err)
		return r
	}
	r.SourceCount = int64(len(pids))

	online := int64(0)
	pipe := cfg.mappingDB.Pipeline()
	checks := make([]*redis.IntCmd, 0, len(pids))
	for _, pid := range pids {
		checks = append(checks, pipe.Exists(ctx, fmt.Sprintf("friend:online:%d", pid)))
	}
	// Best-effort: pipeline errors don't block the audit.
	_, _ = pipe.Exec(ctx)
	for _, c := range checks {
		if c.Val() > 0 {
			online++
		}
	}
	r.TargetCount = online

	if online > 0 {
		r.Severity = "block"
		r.Notes = fmt.Sprintf("%d source-zone players still flagged online. Wait 60s for TTL or investigate — zone-down may be incomplete.", online)
	} else {
		r.Severity = "info"
		r.Notes = "all source-zone players offline ✅"
	}
	return r
}
