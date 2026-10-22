package data

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"net"
	"os"
	"strconv"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	mailpb "proto/mail"
	rankpb "proto/rank"
	"schemamigrate"
	"shared/authoritysql"
)

func rankFixture(t *testing.T) (context.Context, *sql.DB, NextID) {
	t.Helper()
	address := os.Getenv("MAIL_RANK_TEST_TIDB")
	if address == "" {
		if os.Getenv("MAIL_RANK_REQUIRE_TESTS") == "1" {
			t.Fatal("MAIL_RANK_TEST_TIDB必须显式配置本任务独占TiDB")
		}
		t.Skip("需显式本任务数值loopback TiDB端点，随机自有库")
	}
	if err := rankDataFixtureEndpoint(address); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	t.Cleanup(cancel)
	var random [8]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	database := "rank_it_" + hex.EncodeToString(random[:])
	admin, err := authoritysql.Open(ctx, "root@tcp("+address+")/mysql", "mysql", true, 2)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = admin.ExecContext(ctx, "CREATE DATABASE "+database); err != nil {
		admin.Close()
		t.Fatal(err)
	}
	t.Cleanup(func() {
		defer admin.Close()
		clean, stop := context.WithTimeout(context.Background(), 5*time.Second)
		defer stop()
		if _, e := admin.ExecContext(clean, "DROP DATABASE "+database); e != nil {
			t.Errorf("清随机库: %v", e)
		}
	})
	db, err := authoritysql.Open(ctx, "root@tcp("+address+")/"+database, database, true, 8)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	report, err := schemamigrate.Up(ctx, db, schemamigrate.Options{Database: database, Tables: Tables()})
	if err != nil || len(report.Manual) > 0 {
		t.Fatalf("schema: %v %+v", err, report)
	}
	var next atomic.Uint64
	return ctx, db, func(context.Context) (uint64, error) { return next.Add(1), nil }
}

func rankDataFixtureEndpoint(address string) error {
	host, port, err := net.SplitHostPort(address)
	if err != nil {
		return errors.New("排行榜夹具必须显式数值loopback端口")
	}
	ip := net.ParseIP(host)
	number, err := strconv.Atoi(port)
	if ip == nil || !ip.IsLoopback() || err != nil || number < 1 || number > 65535 {
		return errors.New("排行榜夹具仅接受数值loopback与合法非零端口")
	}
	return nil
}
func TestRankDataFixtureEndpointRejectsForeignOrInvalidPort(t *testing.T) {
	for _, address := range []string{"127.0.0.1:24000", "[::1]:24000"} {
		if err := rankDataFixtureEndpoint(address); err != nil {
			t.Fatal(err)
		}
	}
	for _, address := range []string{"localhost:24000", "10.0.0.1:24000", "127.0.0.1:0", "127.0.0.1:65536", "127.0.0.1:mysql", "127.0.0.1"} {
		if rankDataFixtureEndpoint(address) == nil {
			t.Fatal("非法夹具端点未拒绝")
		}
	}
}
func testOpen(t *testing.T, ctx context.Context, r *Repo) *rankpb.OpenRankGenerationResponse {
	t.Helper()
	out, err := r.Open(ctx, "match", &rankpb.OpenRankGenerationRequest{Board: &rankpb.RankBoardKey{BoardType: 1, Scope: rankpb.RankScope_RANK_SCOPE_GLOBAL, Period: "season-1", Generation: 1}, Producer: "match", SourceEpoch: 11, RequestId: 1})
	if err != nil {
		t.Fatal(err)
	}
	return out
}
func TestTiDBOrderedScoresFreezeAndReliableRewardIntents(t *testing.T) {
	ctx, db, next := rankFixture(t)
	r := New(db, next, next, next, func(uint32) bool { return true })
	board := testOpen(t, ctx, r)
	input := &rankpb.SubmitRankScoresRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, SourceCursor: 1, RequestId: 10, Mode: rankpb.RankSubmitMode_RANK_SUBMIT_SET, Entries: []*rankpb.RankScoreInput{{EntityId: 7, Score: 100, AchievedMs: 10}, {EntityId: 8, Score: 100, AchievedMs: 20}, {EntityId: 3, Score: 90, AchievedMs: 30}}}
	if _, err := r.Submit(ctx, "other", input); !errors.Is(err, ErrDenied) {
		t.Fatalf("未授权写分: %v", err)
	}
	if _, err := r.Submit(ctx, "match", input); err != nil {
		t.Fatal(err)
	}
	replayed, err := r.Submit(ctx, "match", input)
	if err != nil || !replayed.Duplicated {
		t.Fatalf("重复cursor: %v %+v", err, replayed)
	}
	changed := proto.Clone(input).(*rankpb.SubmitRankScoresRequest)
	changed.Entries[0].Score++
	if _, err = r.Submit(ctx, "match", changed); !errors.Is(err, ErrConflict) {
		t.Fatalf("篡改cursor: %v", err)
	}
	changed = proto.Clone(input).(*rankpb.SubmitRankScoresRequest)
	changed.SourceCursor = 3
	changed.RequestId = 12
	if _, err = r.Submit(ctx, "match", changed); !errors.Is(err, ErrCursor) {
		t.Fatalf("缺口cursor: %v", err)
	}
	page, err := r.Page(ctx, 3, &rankpb.GetRankPageRequest{Board: &rankpb.RankBoardKey{BoardType: 1, Scope: rankpb.RankScope_RANK_SCOPE_GLOBAL, Period: "season-1"}, Page: 2, PageSize: 1})
	if err != nil || page.TotalCount != 3 || len(page.Entries) != 1 || page.Entries[0].EntityId != 8 || page.Self.GetRank() != 3 {
		t.Fatalf("权威分页: %v %+v", err, page)
	}
	freeze := &rankpb.FreezeRankGenerationRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, ExpectedSourceCursor: 2, RequestId: 20, TopN: 2, Rewards: []*rankpb.RankRewardTier{{FirstRank: 1, LastRank: 2, Attachments: &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{CurrencyType: 0, Amount: 10}}}}}}
	if _, err = r.Freeze(ctx, "match", freeze); !errors.Is(err, ErrCursor) {
		t.Fatalf("未齐输入不可seal: %v", err)
	}
	freeze.ExpectedSourceCursor = 1
	settled, err := r.Freeze(ctx, "match", freeze)
	if err != nil || settled.WinnerCount != 2 {
		t.Fatalf("seal: %v %+v", err, settled)
	}
	for _, table := range []string{"rank_snapshot", "rank_mail_reward"} {
		var count int
		if err = db.QueryRowContext(ctx, "SELECT COUNT(*) FROM "+table).Scan(&count); err != nil || count != 2 {
			t.Fatalf("完整%s: %d %v", table, count, err)
		}
	}
	replay, err := r.Freeze(ctx, "match", freeze)
	if err != nil || !replay.Duplicated || replay.SettlementId != settled.SettlementId {
		t.Fatalf("seal回放: %v %+v", err, replay)
	}
	changed = proto.Clone(input).(*rankpb.SubmitRankScoresRequest)
	changed.SourceCursor = 2
	changed.RequestId = 11
	if _, err = r.Submit(ctx, "match", changed); !errors.Is(err, ErrClosed) {
		t.Fatalf("seal后写入: %v", err)
	}
	if err = r.Remove(ctx, "match", &rankpb.RemoveRankEntryRequest{BoardId: board.BoardId, Generation: 1, RequestId: 30}, true); err != nil {
		t.Fatal(err)
	}
	if _, err = r.Page(ctx, 3, &rankpb.GetRankPageRequest{Board: page.Board}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("隐藏榜仍可查: %v", err)
	}
	var count int
	if err = db.QueryRowContext(ctx, "SELECT COUNT(*) FROM rank_mail_reward").Scan(&count); err != nil || count != 2 {
		t.Fatal("隐藏不得删除奖励意图")
	}
}
func TestTiDBFreezeFailureRollsBackEverySnapshotAndIntent(t *testing.T) {
	ctx, db, next := rankFixture(t)
	r := New(db, next, next, func(context.Context) (uint64, error) { return 999, nil }, func(uint32) bool { return true })
	board := testOpen(t, ctx, r)
	_, err := r.Submit(ctx, "match", &rankpb.SubmitRankScoresRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, SourceCursor: 1, RequestId: 10, Mode: rankpb.RankSubmitMode_RANK_SUBMIT_SET, Entries: []*rankpb.RankScoreInput{{EntityId: 7, Score: 100, AchievedMs: 1}, {EntityId: 8, Score: 90, AchievedMs: 1}}})
	if err != nil {
		t.Fatal(err)
	}
	freeze := &rankpb.FreezeRankGenerationRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, ExpectedSourceCursor: 1, RequestId: 20, TopN: 2, Rewards: []*rankpb.RankRewardTier{{FirstRank: 1, LastRank: 2, Attachments: &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{Amount: 10}}}}}}
	if _, err = r.Freeze(ctx, "match", freeze); err == nil {
		t.Fatal("第二条奖励ID碰撞应使整个seal回滚")
	}
	for _, table := range []string{"rank_settlement", "rank_snapshot", "rank_mail_reward"} {
		var count int
		if err = db.QueryRowContext(ctx, "SELECT COUNT(*) FROM "+table).Scan(&count); err != nil || count != 0 {
			t.Fatalf("半结算泄漏%s count%d err%v", table, count, err)
		}
	}
	var state uint32
	if err = db.QueryRowContext(ctx, "SELECT state FROM rank_board WHERE board_id=?", board.BoardId).Scan(&state); err != nil || state != 1 {
		t.Fatalf("未完整提交不得封榜: %v %d", err, state)
	}
	fixed := New(db, next, next, next, func(uint32) bool { return true })
	if _, err = fixed.Freeze(ctx, "match", freeze); err != nil {
		t.Fatalf("回滚后相同请求可完成: %v", err)
	}
}
