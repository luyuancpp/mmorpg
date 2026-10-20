package main

import (
	"context"
	"strings"
	"testing"

	"data_service/cmd/debugutil"
	componentpb "proto/common/component"
	databasepb "proto/common/database"

	"shared/savebarrier"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

func strEntry(key, text string) *debugutil.RedisEntry {
	value := debugutil.RenderText([]byte(text))
	return &debugutil.RedisEntry{Key: key, Type: "string", Value: &value}
}

func noneEntry(key string) *debugutil.RedisEntry {
	return &debugutil.RedisEntry{Key: key, Type: "none", PTTLMs: -2}
}

func routingCase(home, rec *debugutil.RedisEntry, fallback, configZone uint32, override string) routingInput {
	return routingInput{home: home, placement: rec, fallbackFlag: fallback, configZone: configZone, override: override}
}

func TestResolveBundleRouting(t *testing.T) {
	homeKey, recKey := "player:zone:1", "player:placement:1"
	t.Run("placement wins", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting(routingCase(strEntry(homeKey, "5"), strEntry(recKey, "1000000:3"), 0, 1, ""))
		if err != nil || r.StorageDB != "player_store_1000000_db" || r.ResolvedBy != "placement" || r.HomeZone != 5 || len(warnings) != 0 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("home zone", func(t *testing.T) {
		r, _, err := resolveBundleRouting(routingCase(strEntry(homeKey, "5"), noneEntry(recKey), 9, 1, ""))
		if err != nil || r.StorageDB != "zone_5_db" || r.ResolvedBy != "home_zone" {
			t.Fatalf("got %+v %v", r, err)
		}
	})
	t.Run("fallback warns", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting(routingCase(noneEntry(homeKey), noneEntry(recKey), 0, 3, ""))
		if err != nil || r.StorageDB != "zone_3_db" || r.ResolvedBy != "fallback_zone" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("frozen placement warns", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting(routingCase(strEntry(homeKey, "5"), strEntry(recKey, "6:2:frozen:run1"), 0, 1, ""))
		if err != nil || !r.PlacementFrozen || r.StorageDB != "zone_6_db" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("override is recorded", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting(routingCase(strEntry(homeKey, "5"), noneEntry(recKey), 0, 1, "zone_8_db"))
		if err != nil || r.StorageDB != "zone_8_db" || r.ResolvedBy != "override" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("wrong type counts as absent like MGET", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting(routingCase(strEntry(homeKey, "5"), &debugutil.RedisEntry{Key: recKey, Type: "hash"}, 0, 1, ""))
		if err != nil || r.ResolvedBy != "home_zone" || r.Malformed != "" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	// go/db parsePlacementSnapshot 对这些值 fail-closed;工具不能静默选出一个看似正常的库。
	for name, in := range map[string]routingInput{
		"home not numeric":           routingCase(strEntry(homeKey, "abc"), noneEntry(recKey), 0, 1, ""),
		"home outside zone range":    routingCase(strEntry(homeKey, "1000000"), noneEntry(recKey), 0, 1, ""),
		"malformed home with record": routingCase(strEntry(homeKey, "1000001"), strEntry(recKey, "5:1"), 0, 1, ""),
		"empty placement":            routingCase(strEntry(homeKey, "5"), strEntry(recKey, ""), 0, 1, ""),
		"binary placement":           routingCase(strEntry(homeKey, "5"), strEntry(recKey, "\xff\xfe"), 0, 1, ""),
		"garbage placement":          routingCase(strEntry(homeKey, "5"), strEntry(recKey, "x"), 0, 1, ""),
	} {
		t.Run(name, func(t *testing.T) {
			r, warnings, err := resolveBundleRouting(in)
			if err != nil || r.Malformed == "" || r.StorageDB != "" || len(warnings) == 0 {
				t.Fatalf("malformed routing not flagged: %+v %v %v", r, warnings, err)
			}
		})
	}
	t.Run("malformed with explicit database still reads", func(t *testing.T) {
		r, _, err := resolveBundleRouting(routingCase(strEntry(homeKey, "1000001"), noneEntry(recKey), 0, 1, "zone_5_db"))
		if err != nil || r.Malformed == "" || r.StorageDB != "zone_5_db" {
			t.Fatalf("got %+v %v", r, err)
		}
	})
	t.Run("required without record warns, keeps home db", func(t *testing.T) {
		in := routingCase(strEntry(homeKey, "5"), noneEntry(recKey), 0, 1, "")
		in.required = true
		r, warnings, err := resolveBundleRouting(in)
		if err != nil || r.StorageDB != "zone_5_db" || len(warnings) != 1 || !strings.Contains(warnings[0], "missing_required") {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("nothing determinable fails", func(t *testing.T) {
		if _, _, err := resolveBundleRouting(routingCase(noneEntry(homeKey), noneEntry(recKey), 0, 0, "")); err == nil {
			t.Fatal("no zone at all must fail")
		}
	})
}

func TestBarrierConclusion(t *testing.T) {
	v := savebarrier.Version(3, 9)
	older := savebarrier.Version(3, 8)
	cases := []struct {
		state debugutil.BarrierState
		want  string
	}{
		{debugutil.BarrierState{}, "新号"},
		{debugutil.BarrierState{OwnerEpoch: "4"}, "冷加载会拒绝"},
		{debugutil.BarrierState{Pending: v, PlayerDatabaseAck: v, PlayerDatabase1Ack: v}, "都已落库"},
		{debugutil.BarrierState{Pending: v, PlayerDatabaseAck: v, PlayerDatabase1Ack: v, SavePendingIndexed: true}, "待落库索引"},
		{debugutil.BarrierState{Pending: v, PlayerDatabaseAck: v, PlayerDatabase1Ack: older}, "落后于父快照"},
		{debugutil.BarrierState{PlayerDatabaseAck: v}, "屏障异常"},
	}
	for _, c := range cases {
		if got := barrierConclusion(c.state); !strings.Contains(got, c.want) {
			t.Fatalf("%+v -> %q, want contains %q", c.state, got, c.want)
		}
	}
}

func TestDifferentFieldsSkipsFence(t *testing.T) {
	a := &databasepb.PlayerDatabase{PlayerId: 1, FenceOwnerEpoch: 2, FenceSaveSeq: 3}
	b := proto.Clone(a).(*databasepb.PlayerDatabase)
	b.FenceOwnerEpoch, b.FenceSaveSeq = 9, 9
	if diff := differentFields(a, b, fenceFields); len(diff) != 0 {
		t.Fatalf("fence-only difference reported: %v", diff)
	}
	b.PlayerId = 2
	if diff := differentFields(a, b, fenceFields); len(diff) != 1 || diff[0] != "player_id" {
		t.Fatalf("diff = %v", diff)
	}
}

func TestMergeTransactionLogs(t *testing.T) {
	logs := []*databasepb.TransactionLog{
		{TxId: 1, TimestampSec: 10}, {TxId: 2, TimestampSec: 30},
		{TxId: 2, TimestampSec: 30}, // 自己转自己:两侧查询都读到同一行
		{TxId: 3, TimestampSec: 20}, {TxId: 4, TimestampSec: 30},
	}
	merged := mergeTransactionLogs(logs, 3)
	var got []uint64
	for _, entry := range merged {
		got = append(got, entry.GetTxId())
	}
	if len(got) != 3 || got[0] != 4 || got[1] != 2 || got[2] != 3 {
		t.Fatalf("merged = %v", got)
	}
}

func TestEnsureReadOnlyQueryRejectsSideEffectReads(t *testing.T) {
	for _, query := range []string{
		"EXPLAIN ANALYZE DELETE FROM player_database",
		"SELECT * FROM player_database INTO OUTFILE '/tmp/x'",
		"select * from player_database for update",
		"SELECT * FROM t LOCK IN SHARE MODE",
		"SELECT GET_LOCK('x', 10)",
		"SELECT SLEEP(100)",
		"SELECT 1 INTO @v",
	} {
		if err := ensureReadOnlyQuery(query); err == nil {
			t.Fatalf("expected rejection: %q", query)
		}
	}
	if err := ensureReadOnlyQuery("SELECT * FROM mmorpg_global.transaction_log WHERE from_player = 1"); err != nil {
		t.Fatalf("plain cross-database select rejected: %v", err)
	}
}

func TestReadBundleRedisIncludesRedactedAccount(t *testing.T) {
	mr := miniredis.RunT(t)
	client := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer client.Close()

	accounts, err := proto.Marshal(&databasepb.UserAccounts{Account: "wx_abc", Password: "hash"})
	if err != nil {
		t.Fatal(err)
	}
	mr.Set(debugutil.PlayerToAccountKey(7), "wx_abc")
	mr.Set(debugutil.AccountKey("wx_abc"), string(accounts))

	entries, err := readBundleRedis(context.Background(), client, client, 7, 2, true)
	if err != nil {
		t.Fatal(err)
	}
	account := debugutil.FindRedisEntry(entries, debugutil.AccountKey("wx_abc"))
	if account == nil || account.Value == nil || account.Value.RawBase64 != "" {
		t.Fatalf("account entry missing or carries raw bytes: %+v", account)
	}
	if debugutil.FindRedisEntry(entries, debugutil.SavePendingIndexKey(2, 7)) == nil {
		t.Fatal("home-scoped save_pending key not read")
	}

	without, err := readBundleRedis(context.Background(), client, client, 7, 2, false)
	if err != nil {
		t.Fatal(err)
	}
	if debugutil.FindRedisEntry(without, debugutil.AccountKey("wx_abc")) != nil {
		t.Fatal("-skip-account still read the account")
	}
}

// 父快照里 C++ 留下的「存在但为空」的组件,经 go/db 落库读回后是「未设置」;两者不应报差异。
func TestDifferentFieldsTreatsEmptySubMessageAsUnset(t *testing.T) {
	parentSide := &databasepb.PlayerDatabase{PlayerId: 1, Currency: &componentpb.CurrencyComp{}}
	sqlSide := &databasepb.PlayerDatabase{PlayerId: 1}
	if diff := differentFields(parentSide, sqlSide, fenceFields); len(diff) != 0 {
		t.Fatalf("empty vs unset reported as different: %v", diff)
	}
	parentSide.Currency = &componentpb.CurrencyComp{Values: []uint64{5}}
	if diff := differentFields(parentSide, sqlSide, fenceFields); len(diff) != 1 || diff[0] != "currency" {
		t.Fatalf("real difference not reported: %v", diff)
	}
}

func TestBarrierAnomalies(t *testing.T) {
	v := savebarrier.Version(5, 9)
	aligned := debugutil.BarrierState{OwnerEpoch: "5", Pending: v, PlayerDatabaseAck: v, PlayerDatabase1Ack: v,
		PlayerDatabaseFence: v, PlayerDatabase1Fence: v, ParentPresent: true, ParentLastSaveSeq: 9}
	if got := barrierAnomalies(aligned, true); len(got) != 0 {
		t.Fatalf("aligned state flagged: %v", got)
	}

	rolledBack := aligned
	rolledBack.PlayerDatabaseFence = savebarrier.Version(5, 8)
	if got := barrierAnomalies(rolledBack, true); len(got) != 1 || !strings.Contains(got[0], "player_database 行围栏") {
		t.Fatalf("rolled-back SQL fence not flagged: %v", got)
	}
	if got := barrierAnomalies(rolledBack, false); len(got) != 0 {
		t.Fatalf("SQL comparison must be skipped when the DB was not read: %v", got)
	}

	ownerBehind := aligned
	ownerBehind.OwnerEpoch = "4"
	if got := barrierAnomalies(ownerBehind, true); len(got) != 1 || !strings.Contains(got[0], "owner_epoch") {
		t.Fatalf("owner behind pending not flagged: %v", got)
	}

	watermark := aligned
	watermark.ParentLastSaveSeq = 7
	if got := barrierAnomalies(watermark, true); len(got) != 1 || !strings.Contains(got[0], "last_save_seq") {
		t.Fatalf("parent watermark mismatch not flagged: %v", got)
	}

	terminal := debugutil.BarrierState{Pending: v, PlayerDatabaseAck: savebarrier.Version(5, 8), OutboxTerminal: v, OutboxTerminalReason: "permanent"}
	if got := barrierConclusion(terminal); !strings.Contains(got, "补投已终止") {
		t.Fatalf("terminal outbox not reported: %q", got)
	}
}

func TestEnsureReadOnlyQueryRejectsCommentBypass(t *testing.T) {
	for _, query := range []string{
		"EXPLAIN ANALYZE/**/DELETE FROM player_database",
		"EXPLAIN /**/ANALYZE DELETE FROM t",
		"select * from t for/**/update",
		"SELECT * FROM t -- for update",
		"SELECT * FROM t # x",
		"SELECT SLEEP (1)",
		"SELECT * FROM t\nFOR\nUPDATE",
		"SELECT * FROM t INTO OUTFILE'/tmp/x'",
	} {
		if err := ensureReadOnlyQuery(query); err == nil {
			t.Fatalf("expected rejection: %q", query)
		}
	}
}

func TestSameRevision(t *testing.T) {
	if !sameRevision("bf3c606", "bf3c60632aa+dirty") || sameRevision("bf3c606", "e7bb460") || sameRevision("", "x") {
		t.Fatal("sameRevision wrong")
	}
}
