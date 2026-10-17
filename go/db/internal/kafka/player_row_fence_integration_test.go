package kafka

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"os"
	"sync"
	"testing"
	"time"

	db_config "db/internal/config"

	"github.com/go-sql-driver/mysql"
	"github.com/luyuancpp/proto2mysql"
	"google.golang.org/protobuf/proto"
	componentpb "proto/common/component"
	playerpb "proto/common/database"
	dbpb "proto/db"
	"shared/authoritysql"
)

// 只在一次性随机库验证；不借用已有 zone/player 库，不改共享 SQL/Redis 数据。
func openAssetFenceFixture(t *testing.T) *proto2mysql.DB {
	t.Helper()
	dsn := os.Getenv("ASSET_FENCE_TEST_DSN")
	if dsn == "" {
		t.Skip("未配置独立 ASSET_FENCE_TEST_DSN")
	}
	parsed, err := mysql.ParseDSN(dsn)
	if err != nil || parsed.DBName != "" {
		t.Fatal("fixture DSN 必须不指定数据库")
	}
	admin, err := sql.Open("mysql", dsn)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = admin.Close() })
	var nonce [8]byte
	if _, err = rand.Read(nonce[:]); err != nil {
		t.Fatal(err)
	}
	name := "asset_fence_" + hex.EncodeToString(nonce[:])
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	if _, err = admin.ExecContext(ctx, "CREATE DATABASE `"+name+"`"); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if _, err := admin.Exec("DROP DATABASE `" + name + "`"); err != nil {
			t.Errorf("清理 fixture 库失败: %v", err)
		}
	})
	parsed.DBName = name
	handle, err := authoritysql.Open(ctx, parsed.FormatDSN(), name, os.Getenv("ASSET_FENCE_REQUIRE_TIDB") == "1", 8)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = handle.Close() })
	model := proto2mysql.NewDB()
	if err = model.OpenDB(handle, name); err != nil {
		t.Fatal(err)
	}
	// 这些用例验证 dual 下的行内围栏;缺省 legacy 时 savePlayerRow 直接让调用者走普通 Save。
	if err := SetFencingMode(db_config.FencingModeDual); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = SetFencingMode(db_config.FencingModeLegacy) })
	model.RegisterTable(&playerpb.PlayerDatabase{})
	model.RegisterTable(&playerpb.PlayerDatabase_1{})
	if err = model.CreateOrUpdateTable(&playerpb.PlayerDatabase{}); err != nil {
		t.Fatal(err)
	}
	if err = model.CreateOrUpdateTable(&playerpb.PlayerDatabase_1{}); err != nil {
		t.Fatal(err)
	}
	return model
}

func fenceRow(balance uint64) *playerpb.PlayerDatabase {
	return &playerpb.PlayerDatabase{PlayerId: 42, Currency: &componentpb.CurrencyComp{Values: []uint64{0, balance}}}
}

func writeFence(t *testing.T, model *proto2mysql.DB, epoch, seq, balance uint64) error {
	t.Helper()
	handled, err := savePlayerRow(context.Background(), model, &dbpb.DBTask{Key: 42, OwnerEpoch: epoch, SaveSeq: seq}, fenceRow(balance))
	if !handled {
		t.Fatal("玩家行未进入 SQL 围栏")
	}
	return err
}

func TestIntegrationPlayerSQLFenceRejectsLateAndConflictingWrites(t *testing.T) {
	model := openAssetFenceFixture(t)
	if err := writeFence(t, model, 10, 1, 100); err != nil {
		t.Fatal(err)
	}
	if err := writeFence(t, model, 10, 1, 100); err != nil {
		t.Fatalf("同版本同载荷应幂等: %v", err)
	}
	// 同 (epoch, seq) 按幂等生产者口径视为重放，不比内容（v0.4 §14 第 12 条），以先提交的 SQL 行为准。
	if err := writeFence(t, model, 10, 1, 99); err != nil {
		t.Fatalf("同版本重放不得报错: %v", err)
	}
	replayed := &playerpb.PlayerDatabase{PlayerId: 42}
	if err := model.FindOneByPK(replayed); err != nil {
		t.Fatal(err)
	}
	if replayed.Currency.Values[1] != 100 {
		t.Fatalf("同版本重放不得改写 SQL 行: %v", replayed)
	}
	if err := writeFence(t, model, 10, 2, 80); err != nil {
		t.Fatal(err)
	}
	for _, v := range [][2]uint64{{10, 1}, {10, 0}, {0, 0}, {9, 999999}} {
		if err := writeFence(t, model, v[0], v[1], 999); !errors.Is(err, errPlayerRowStale) {
			t.Fatalf("旧存盘必须拒绝 %v: %v", v, err)
		}
	}
	if err := writeFence(t, model, 11, 1, 70); err != nil {
		t.Fatal(err)
	}
	if err := writeFence(t, model, 10, 999999, 999); !errors.Is(err, errPlayerRowStale) {
		t.Fatalf("旧 owner 的大 seq 不能反超: %v", err)
	}
	got := &playerpb.PlayerDatabase{PlayerId: 42}
	if err := model.FindOneByPK(got); err != nil {
		t.Fatal(err)
	}
	if got.FenceOwnerEpoch != 11 || got.FenceSaveSeq != 1 || got.Currency.Values[1] != 70 {
		t.Fatalf("SQL 状态被旧写覆盖: %v", got)
	}
}

func TestIntegrationPlayerSQLFenceSerializesConcurrentSaves(t *testing.T) {
	model := openAssetFenceFixture(t)
	if err := writeFence(t, model, 12, 1, 1); err != nil {
		t.Fatal(err)
	}
	var group sync.WaitGroup
	failures := make(chan error, 16)
	for seq := uint64(2); seq <= 17; seq++ {
		group.Add(1)
		go func(seq uint64) {
			defer group.Done()
			_, err := savePlayerRow(context.Background(), model, &dbpb.DBTask{Key: 42, OwnerEpoch: 12, SaveSeq: seq}, fenceRow(seq))
			if err != nil && !errors.Is(err, errPlayerRowStale) {
				failures <- err
			}
		}(seq)
	}
	group.Wait()
	close(failures)
	for err := range failures {
		t.Error(err)
	}
	got := &playerpb.PlayerDatabase{PlayerId: 42}
	if err := model.FindOneByPK(got); err != nil {
		t.Fatal(err)
	}
	if got.FenceSaveSeq != 17 || got.Currency.Values[1] != 17 {
		t.Fatalf("最终只能保留最大保存版本: %v", got)
	}
	cancelled, cancel := context.WithCancel(context.Background())
	cancel()
	before := proto.Clone(got)
	if _, err := savePlayerRow(cancelled, model, &dbpb.DBTask{Key: 42, OwnerEpoch: 12, SaveSeq: 18}, fenceRow(18)); err == nil {
		t.Fatal("取消的写不得提交")
	}
	got = &playerpb.PlayerDatabase{PlayerId: 42}
	if err := model.FindOneByPK(got); err != nil {
		t.Fatal(err)
	}
	if !proto.Equal(before, got) {
		t.Fatal("取消改变了 SQL 状态")
	}
}

func fenceRowFor(playerID, balance uint64) *playerpb.PlayerDatabase {
	return &playerpb.PlayerDatabase{PlayerId: playerID, Currency: &componentpb.CurrencyComp{Values: []uint64{0, balance}}}
}

// 不同新号并发首次存盘不得跨玩家死锁（RR 下锁定读未命中取间隙锁 → 两边插入意向锁互等 → 1213）。
// 空表（压测清库后的形态）与非空表各跑一轮；旧实现在两种形态下都能真库复现 1213。
func TestIntegrationPlayerSQLFenceConcurrentFirstSavesOfDifferentPlayersDoNotDeadlock(t *testing.T) {
	model := openAssetFenceFixture(t)
	runRound := func(base uint64) {
		const players = 16
		var group sync.WaitGroup
		start := make(chan struct{})
		failures := make(chan error, players)
		for i := uint64(0); i < players; i++ {
			group.Add(1)
			go func(playerID uint64) {
				defer group.Done()
				<-start
				_, err := savePlayerRow(context.Background(), model, &dbpb.DBTask{Key: playerID, OwnerEpoch: 7, SaveSeq: 1}, fenceRowFor(playerID, playerID))
				if err != nil {
					failures <- err
				}
			}(base + i)
		}
		close(start)
		group.Wait()
		close(failures)
		for err := range failures {
			t.Errorf("新号首次存盘失败（疑似跨玩家死锁）: %v", err)
		}
		for i := uint64(0); i < players; i++ {
			got := &playerpb.PlayerDatabase{PlayerId: base + i}
			if err := model.FindOneByPK(got); err != nil {
				t.Fatalf("player %d 未落库: %v", base+i, err)
			}
			if got.FenceOwnerEpoch != 7 || got.FenceSaveSeq != 1 || got.Currency.Values[1] != base+i {
				t.Fatalf("player %d 落库内容不对: %v", base+i, got)
			}
		}
	}
	runRound(1000) // 空表
	runRound(500)  // 非空表，新号落在已有行之间的间隙
}

// 同一新号的多笔首写并发：只有一笔走插入，其余撞 1062 后转锁定比较，最终保留最大版本。
func TestIntegrationPlayerSQLFenceConcurrentFirstSavesOfSamePlayerKeepMaxVersion(t *testing.T) {
	model := openAssetFenceFixture(t)
	const playerID = 77
	var group sync.WaitGroup
	start := make(chan struct{})
	failures := make(chan error, 8)
	for seq := uint64(1); seq <= 8; seq++ {
		group.Add(1)
		go func(seq uint64) {
			defer group.Done()
			<-start
			task := &dbpb.DBTask{Key: playerID, OwnerEpoch: 3, SaveSeq: seq}
			_, err := savePlayerRow(context.Background(), model, task, fenceRowFor(playerID, seq))
			// 已登记的残余窗口（v0.4 §4.8 先行修复注）：同一玩家首个自动提交 INSERT 回滚、后面排着 ≥2 笔时
			// 会 1213。生产上同一玩家的写由 key 分片加排序租约串行到不了；这里 8 路故意并发，按管线的重试吸收一次。
			var myErr *mysql.MySQLError
			if errors.As(err, &myErr) && myErr.Number == 1213 {
				_, err = savePlayerRow(context.Background(), model, task, fenceRowFor(playerID, seq))
			}
			if err != nil && !errors.Is(err, errPlayerRowStale) {
				failures <- err
			}
		}(seq)
	}
	close(start)
	group.Wait()
	close(failures)
	for err := range failures {
		t.Error(err)
	}
	got := &playerpb.PlayerDatabase{PlayerId: playerID}
	if err := model.FindOneByPK(got); err != nil {
		t.Fatal(err)
	}
	if got.FenceSaveSeq != 8 || got.Currency.Values[1] != 8 {
		t.Fatalf("最终只能保留最大保存版本: %v", got)
	}
}
