package main

import (
	"context"
	"testing"
)

// 玩家收件箱的合服审计(player_inbox_audit.go)。有库的分支(缺列 block / 表不在 warn / info)走真 MySQL,
// 属集成测试范围;这里钉住不依赖外部状态的两条:没有句柄必须是 INFRA,以及它只登记在合服前门禁组。

func TestAuditPlayerInbox_NoMySQLHandleIsInfra(t *testing.T) {
	r := auditPlayerInbox(context.Background(), auditConfig{src: 901, dst: 902})
	if r.Severity != "block" || !isInfraAudit(r) {
		t.Fatalf("no MySQL handle must be an INFRA block (exit 2), got %+v", r)
	}
}

func TestRunAuditMode_PlayerInboxOnlyInPreMergeAudit(t *testing.T) {
	// 全部句柄为 nil:每个 auditor 都应返回 INFRA 行而不是 panic。这里只关心 player_inbox 行登记在哪一组 ——
	// 缺账本列要在维护窗口之前拦下;合服后条目不随 zone 走,没有收件箱特有的断言。
	has := func(rs []ResourceAudit) bool {
		for _, r := range rs {
			if r.Name == "player_inbox" {
				return true
			}
		}
		return false
	}
	ctx := context.Background()
	if !has(runAuditMode(ctx, auditConfig{verify: false, tradeSchema: defaultTradeSchema, src: 901, dst: 902})) {
		t.Error("the pre-merge audit must include player_inbox (inbox_ledger column gate)")
	}
	if has(runAuditMode(ctx, auditConfig{verify: true, tradeSchema: defaultTradeSchema,
		guildSchema: defaultGuildSchema, src: 901, dst: 902})) {
		t.Error("-verify-merged must not include player_inbox")
	}
}

func TestZoneDBNameMatchesLedgerAuditTarget(t *testing.T) {
	// 账本列审计按 zoneDBName 拼库名;与 go/db 的 ZoneDBName 口径一致(zone_<N>_db)。
	if got := zoneDBName(901); got != "zone_901_db" {
		t.Fatalf("zoneDBName(901) = %q, want zone_901_db", got)
	}
}
