package dataloader

import (
	"context"
	dbpb "proto/db"
	"shared/ownerepoch"
	"shared/savebarrier"
	"strings"
	"testing"
)

// owner 已铸造不能证明从未存盘。父键、屏障和 SQL 行同时缺失时没有持久新号原件，必须拒绝。
// SQL 回包 Data 确实为空，而不是人为带 player_id 的零围栏行；仍走完整组装和提交调用面。
func TestParentLoadMissingRedisAndSQLHistoryWithMintedOwnerFailsClosed(t *testing.T) {
	mr, rc := newSeedTestRedis(t)
	ctx := context.Background()
	if err := rc.Set(ctx, ownerepoch.OwnerEpochKey(seedTestPlayerID), "1", 0).Err(); err != nil {
		t.Fatal(err)
	}
	setTestParentIdentity(t, rc, "redis-live", "")
	if mr.Exists(seedParentKey(seedTestPlayerID)) || mr.Exists(savebarrier.Key(seedTestPlayerID)) {
		t.Fatal("回归前提不是父键与屏障均缺失")
	}
	before := mr.Dump()
	reads := &coldSQLReads{read: func(*dbpb.DBTask) ([]byte, error) { return nil, nil }}
	calls := 0
	var gotErr error
	ensurePlayerAllDataInRedisAsync(ctx, rc, testParentLoadScripts, true, reads, reads, seedTestPlayerID, 0, func(err error) {
		calls++
		gotErr = err
	})
	if calls != 1 || reads.count != 2 {
		t.Fatalf("没有精确完成两张 SQL 空行核验: calls=%d reads=%d", calls, reads.count)
	}
	if gotErr == nil || !strings.Contains(gotErr.Error(), "历史") {
		t.Errorf("历史全缺不能被推断成新号: %v", gotErr)
	}
	if after := mr.Dump(); after != before {
		t.Fatal("拒绝路径改写了原键，或凭空建立父快照/屏障/身份戳")
	}
}
