package main

import (
	"context"
	"strings"
	"testing"

	"data_service/cmd/debugutil"
	databasepb "proto/common/database"

	"shared/savebarrier"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

func TestResolveBundleRouting(t *testing.T) {
	t.Run("placement wins", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting("5", "1000000:3", 0, 1, "")
		if err != nil || r.StorageDB != "player_store_1000000_db" || r.ResolvedBy != "placement" || r.HomeZone != 5 || len(warnings) != 0 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("home zone", func(t *testing.T) {
		r, _, err := resolveBundleRouting("5", "", 9, 1, "")
		if err != nil || r.StorageDB != "zone_5_db" || r.ResolvedBy != "home_zone" {
			t.Fatalf("got %+v %v", r, err)
		}
	})
	t.Run("fallback warns", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting("", "", 0, 3, "")
		if err != nil || r.StorageDB != "zone_3_db" || r.ResolvedBy != "fallback_zone" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("frozen placement warns", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting("5", "6:2:frozen:run1", 0, 1, "")
		if err != nil || !r.PlacementFrozen || r.StorageDB != "zone_6_db" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("override is recorded", func(t *testing.T) {
		r, warnings, err := resolveBundleRouting("5", "", 0, 1, "zone_8_db")
		if err != nil || r.StorageDB != "zone_8_db" || r.ResolvedBy != "override" || len(warnings) != 1 {
			t.Fatalf("got %+v %v %v", r, warnings, err)
		}
	})
	t.Run("malformed and unknown fail closed", func(t *testing.T) {
		if _, _, err := resolveBundleRouting("abc", "", 0, 1, ""); err == nil {
			t.Fatal("malformed home zone accepted")
		}
		if _, _, err := resolveBundleRouting("5", "x", 0, 1, ""); err == nil {
			t.Fatal("malformed placement accepted")
		}
		if _, _, err := resolveBundleRouting("", "", 0, 0, ""); err == nil {
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
