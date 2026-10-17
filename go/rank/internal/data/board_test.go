package data

// 写 / 读两段 Lua 的真实执行测试:miniredis 内置 gopher-lua,ZADD / ZSCORE / ZREVRANGE / HMGET 与真 Redis 同语义。
// 不依赖外部 Redis,没有 SKIP 分支(设计 §8.1 T11:SKIP 不等于 PASS)。

import (
	"context"
	"errors"
	"math"
	"strconv"
	"strings"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

func newTestBoard(t *testing.T) (*Board, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	return NewRatingBoard(rds), mr
}

// ms 返回 epoch 之后 sec 秒的毫秒时间戳。
func ms(sec uint64) uint64 { return (ScoreEpochSec + sec) * 1000 }

func mustWrite(t *testing.T, b *Board, s Snapshot) WriteOutcome {
	t.Helper()
	out, err := b.Write(context.Background(), s)
	if err != nil {
		t.Fatalf("Write(%+v): %v", s, err)
	}
	return out
}

func zscore(t *testing.T, mr *miniredis.Miniredis, playerID uint64) uint64 {
	t.Helper()
	f, err := mr.ZScore(RatingScoresKey(), strconv.FormatUint(playerID, 10))
	if err != nil {
		t.Fatalf("ZSCORE %d: %v", playerID, err)
	}
	return uint64(f)
}

func version(t *testing.T, mr *miniredis.Miniredis, playerID uint64) string {
	t.Helper()
	return mr.HGet(RatingVersionsKey(), strconv.FormatUint(playerID, 10))
}

// T1:同一代际先写 games=5 再写 games=3 → 第二次 stale,ZSET 仍是 games=5 那次的分数(两次用不同分数,否则"没被覆盖"无法区分)。
func TestWriteRejectsOlderGamesInSameGen(t *testing.T) {
	b, mr := newTestBoard(t)
	if got := mustWrite(t, b, Snapshot{PlayerID: 7, RatingCenti: 152000, Games: 5, UpdatedAtMs: ms(100), RatingGen: 1000}); got != WriteWritten {
		t.Fatalf("首次写入应 written,得到 %v", got)
	}
	if got := mustWrite(t, b, Snapshot{PlayerID: 7, RatingCenti: 149000, Games: 3, UpdatedAtMs: ms(50), RatingGen: 1000}); got != WriteStale {
		t.Fatalf("同代际更小的 games 应 stale,得到 %v", got)
	}
	if rc := DecodeRatingCenti(zscore(t, mr, 7)); rc != 152000 {
		t.Fatalf("ZSET 应仍是 games=5 那次的 152000,得到 %d", rc)
	}
	if v := version(t, mr, 7); v != "1000:5" {
		t.Fatalf("版本应仍是 1000:5,得到 %q", v)
	}
}

// T2:重放同一事件 → stale,什么都不改。
func TestWriteReplayIsStale(t *testing.T) {
	b, mr := newTestBoard(t)
	s := Snapshot{PlayerID: 9, RatingCenti: 151629, Games: 4, UpdatedAtMs: ms(200), RatingGen: 1000}
	mustWrite(t, b, s)
	before := zscore(t, mr, 9)
	if got := mustWrite(t, b, s); got != WriteStale {
		t.Fatalf("重放应 stale,得到 %v", got)
	}
	if after := zscore(t, mr, 9); after != before {
		t.Fatalf("重放改动了 ZSET: %d → %d", before, after)
	}
}

// T3:同分决胜 —— 早达到者在前。ZREVRANGE 对**完全相同**的 score 按 member 字节序**倒序**排(大的在前),
// 所以刻意让早达到者的 member 字节序**更小**("10" < "9"):时间分量失效时 9 会排在前面、本用例变红;
// 反过来构造(早达到者字节序更大)时间分量失效也恒绿。
func TestTieBreakEarlierFirst(t *testing.T) {
	b, _ := newTestBoard(t)
	mustWrite(t, b, Snapshot{PlayerID: 10, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(10), RatingGen: 1000})
	mustWrite(t, b, Snapshot{PlayerID: 9, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(20), RatingGen: 1000})
	page, err := b.ReadPage(context.Background(), 1, 10, 0)
	if err != nil {
		t.Fatal(err)
	}
	if len(page.Entries) != 2 || page.Entries[0].PlayerID != 10 || page.Entries[1].PlayerID != 9 {
		t.Fatalf("同分时早达到的 10 应排第 1(字节序倒序会让 9 在前,这正是要排除的),得到 %+v", page.Entries)
	}
	if page.Entries[0].Rank != 1 || page.Entries[1].Rank != 2 {
		t.Fatalf("名次应连续 1、2,得到 %+v", page.Entries)
	}
}

// T3′:A 在 t1 达到 1500.00,B 在 t2 > t1 达到 1500.00;A 再写一次**同分**快照(games+1、updated 为 t3 > t2)→
// A 的名次不下降(仍在 B 前),版本表里 A 的 games 已更新。没有第三方 B 夹在中间的话,名次不变可能只是巧合。
func TestSameScoreRewriteKeepsAchievedTime(t *testing.T) {
	b, mr := newTestBoard(t)
	const a, bID = 20, 3 // B 的 id 字节序更大("3" > "20"):时间分量被改坏时同分按字节序倒序会让 B 排前,本用例变红
	mustWrite(t, b, Snapshot{PlayerID: a, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(10), RatingGen: 1000})
	mustWrite(t, b, Snapshot{PlayerID: bID, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(20), RatingGen: 1000})
	before := zscore(t, mr, a)
	if got := mustWrite(t, b, Snapshot{PlayerID: a, RatingCenti: 150000, Games: 2, UpdatedAtMs: ms(30), RatingGen: 1000}); got != WriteWritten {
		t.Fatalf("同分新局应 written(版本前进),得到 %v", got)
	}
	if after := zscore(t, mr, a); after != before {
		t.Fatalf("同分重写不应改时间分量: %d → %d", before, after)
	}
	if v := version(t, mr, a); v != "1000:2" {
		t.Fatalf("版本应前进到 1000:2,得到 %q", v)
	}
	page, err := b.ReadPage(context.Background(), 1, 10, 0)
	if err != nil {
		t.Fatal(err)
	}
	if page.Entries[0].PlayerID != a || page.Entries[0].Games != 2 {
		t.Fatalf("A 应仍排第 1 且 games=2,得到 %+v", page.Entries)
	}
	// 分数变了就要按新时刻重算:A 掉到 1499.99 再回到 1500.00,"达到 1500"的时间是回来的那一刻 → 排到 B 后面。
	mustWrite(t, b, Snapshot{PlayerID: a, RatingCenti: 149999, Games: 3, UpdatedAtMs: ms(40), RatingGen: 1000})
	mustWrite(t, b, Snapshot{PlayerID: a, RatingCenti: 150000, Games: 4, UpdatedAtMs: ms(50), RatingGen: 1000})
	page, err = b.ReadPage(context.Background(), 1, 10, 0)
	if err != nil {
		t.Fatal(err)
	}
	if page.Entries[0].PlayerID != bID || page.Entries[1].PlayerID != a {
		t.Fatalf("A 重新达到 1500 晚于 B,应排在 B 后面,得到 %+v", page.Entries)
	}
}

// T12:代际重置。新代际的 games **小于**旧值(否则没有代际也会写进,测不出)→ gen_reset、ZSET 分数为新值;旧代际的迟到事件 → stale。
func TestGenResetOverridesAndOldGenIsStale(t *testing.T) {
	b, mr := newTestBoard(t)
	mustWrite(t, b, Snapshot{PlayerID: 5, RatingCenti: 180000, Games: 50, UpdatedAtMs: ms(100), RatingGen: 1000})
	if got := mustWrite(t, b, Snapshot{PlayerID: 5, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(200), RatingGen: 2000}); got != WriteGenReset {
		t.Fatalf("新代际应 gen_reset,得到 %v", got)
	}
	if rc := DecodeRatingCenti(zscore(t, mr, 5)); rc != 150000 {
		t.Fatalf("gen_reset 后分数应为新值 150000,得到 %d", rc)
	}
	if got := mustWrite(t, b, Snapshot{PlayerID: 5, RatingCenti: 181000, Games: 51, UpdatedAtMs: ms(150), RatingGen: 1000}); got != WriteStale {
		t.Fatalf("旧代际的迟到事件应 stale,得到 %v", got)
	}
	if v := version(t, mr, 5); v != "2000:1" {
		t.Fatalf("版本应是 2000:1,得到 %q", v)
	}
}

// gen_reset 即使分数与旧代际相同也重写时间分量(新代际的"达到时刻"不继承丢失前的历史,设计 §12 偏差)。
func TestGenResetRewritesTimeEvenWithSameScore(t *testing.T) {
	b, mr := newTestBoard(t)
	mustWrite(t, b, Snapshot{PlayerID: 6, RatingCenti: 150000, Games: 9, UpdatedAtMs: ms(10), RatingGen: 1000})
	before := zscore(t, mr, 6)
	mustWrite(t, b, Snapshot{PlayerID: 6, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(500), RatingGen: 2000})
	if after := zscore(t, mr, 6); after == before {
		t.Fatal("gen_reset 应按新代际的时刻重写时间分量")
	}
}

func TestWriteRejectsInvalidAndOutOfRangeWithoutTouchingRedis(t *testing.T) {
	b, mr := newTestBoard(t)
	for _, s := range []Snapshot{
		{PlayerID: 0, RatingCenti: 1, Games: 1, UpdatedAtMs: ms(1), RatingGen: 1},
		{PlayerID: 1, RatingCenti: 1, Games: 0, UpdatedAtMs: ms(1), RatingGen: 1},
		{PlayerID: 1, RatingCenti: 1, Games: 1, UpdatedAtMs: ms(1), RatingGen: 0},
	} {
		if _, err := b.Write(context.Background(), s); !errors.Is(err, ErrInvalidSnapshot) {
			t.Errorf("%+v 应回 ErrInvalidSnapshot,得到 %v", s, err)
		}
	}
	if _, err := b.Write(context.Background(), Snapshot{PlayerID: 1, RatingCenti: MaxRatingCenti + 1, Games: 1, UpdatedAtMs: ms(1), RatingGen: 1}); !errors.Is(err, ErrOutOfRange) {
		t.Errorf("越界分数应回 ErrOutOfRange,得到 %v", err)
	}
	if mr.Exists(RatingScoresKey()) || mr.Exists(RatingVersionsKey()) {
		t.Fatal("非法 / 越界快照不应碰 Redis")
	}
}

// 版本表被手写坏了:写 Lua 回 error_reply,不猜新旧;Write 归成 ErrMalformedVersion(消费者据此跳过,不卡分区)。
func TestWriteMalformedVersionIsError(t *testing.T) {
	b, mr := newTestBoard(t)
	for _, bad := range []string{"garbage", "x:1", "1:y"} {
		mr.HSet(RatingVersionsKey(), "11", bad)
		_, err := b.Write(context.Background(), Snapshot{PlayerID: 11, RatingCenti: 150000, Games: 1, UpdatedAtMs: ms(1), RatingGen: 1})
		if !errors.Is(err, ErrMalformedVersion) {
			t.Fatalf("版本值 %q 坏了应报 ErrMalformedVersion,得到 %v", bad, err)
		}
	}
	// Lua 的 error_reply 前缀与 Go 侧的判别串必须逐字一致,否则坏版本会被当成可重试错误,永远卡住分区。
	if got := strings.Count(writeScript, "'"+malformedVersionReply+" '"); got != 2 {
		t.Fatalf("writeScript 里 error_reply 前缀 %q 出现 %d 次,期望 2", malformedVersionReply, got)
	}
}

func seedPlayers(t *testing.T, b *Board, n int) {
	t.Helper()
	for i := 1; i <= n; i++ {
		// 分数各不相同:玩家 i 的分数 = 100000 + i,名次 = n − i + 1。
		mustWrite(t, b, Snapshot{PlayerID: uint64(i), RatingCenti: uint32(100000 + i), Games: uint64(i), UpdatedAtMs: ms(uint64(i)), RatingGen: 1000})
	}
}

// T5(数据层部分):页码超过末页按末页;溢出输入(page 很大)钳到末页且响应里的 page 是钳制后的值;名次接着页起点连续。
func TestReadPageClampsToLastPage(t *testing.T) {
	b, _ := newTestBoard(t)
	seedPlayers(t, b, 45) // 每页 20 → 3 页,末页 5 条
	for _, page := range []uint32{3, 4, 214748366, math.MaxUint32} {
		got, err := b.ReadPage(context.Background(), page, 20, 0)
		if err != nil {
			t.Fatalf("page=%d: %v", page, err)
		}
		if got.Page != 3 || got.PageCount != 3 || got.Total != 45 || len(got.Entries) != 5 {
			t.Fatalf("page=%d 应钳到第 3 页(5 条),得到 page=%d count=%d total=%d entries=%d", page, got.Page, got.PageCount, got.Total, len(got.Entries))
		}
		if got.Entries[0].Rank != 41 || got.Entries[4].Rank != 45 || got.Entries[4].PlayerID != 1 {
			t.Fatalf("末页名次应是 41..45、最后一名是玩家 1,得到 %+v", got.Entries)
		}
	}
	first, err := b.ReadPage(context.Background(), 1, 20, 0)
	if err != nil {
		t.Fatal(err)
	}
	if first.Entries[0].PlayerID != 45 || first.Entries[0].Rank != 1 || first.Entries[0].RatingCenti != 100045 || first.Entries[0].Games != 45 {
		t.Fatalf("第 1 名应是玩家 45(100045,45 局),得到 %+v", first.Entries[0])
	}
}

// 空榜:page_count = 1、page = 1、entries 为空,不是错误。
func TestReadPageEmptyBoard(t *testing.T) {
	b, _ := newTestBoard(t)
	got, err := b.ReadPage(context.Background(), 5, 20, 42)
	if err != nil {
		t.Fatal(err)
	}
	if got.Page != 1 || got.PageCount != 1 || got.Total != 0 || len(got.Entries) != 0 || got.Self.Rank != 0 {
		t.Fatalf("空榜应是 page=1 count=1 total=0 无条目、self 未上榜,得到 %+v", got)
	}
}

// T6(数据层部分):self 未上榜时 Self.Rank == 0;上榜时 self 的名次与它在页中的位置一致。
// 数据集里"自己"刻意**不在**第一页,否则 self 永远上榜、测不出。
func TestReadPageSelf(t *testing.T) {
	b, _ := newTestBoard(t)
	seedPlayers(t, b, 30)
	got, err := b.ReadPage(context.Background(), 1, 10, 999)
	if err != nil {
		t.Fatal(err)
	}
	if got.Self.Rank != 0 || got.Self.PlayerID != 0 {
		t.Fatalf("不在榜上的 999 应 self.rank=0,得到 %+v", got.Self)
	}
	got, err = b.ReadPage(context.Background(), 1, 10, 5) // 玩家 5 是第 26 名,在第 3 页
	if err != nil {
		t.Fatal(err)
	}
	if got.Self.Rank != 26 || got.Self.PlayerID != 5 || got.Self.RatingCenti != 100005 || got.Self.Games != 5 {
		t.Fatalf("玩家 5 应是第 26 名(100005,5 局),得到 %+v", got.Self)
	}
	page3, err := b.ReadPage(context.Background(), 3, 10, 5)
	if err != nil {
		t.Fatal(err)
	}
	if page3.Entries[5].PlayerID != 5 || page3.Entries[5].Rank != got.Self.Rank {
		t.Fatalf("self 名次应与它在页中的位置一致,得到 entries=%+v self=%+v", page3.Entries, got.Self)
	}
}

// 只有 ZSET 没有版本表(§3.5 的半丢失状态):名次与分数照常,games 记 0,不报错。
func TestReadPageWithMissingVersion(t *testing.T) {
	b, mr := newTestBoard(t)
	score, _ := EncodeScore(150000, ms(1))
	if _, err := mr.ZAdd(RatingScoresKey(), float64(score), "77"); err != nil {
		t.Fatal(err)
	}
	got, err := b.ReadPage(context.Background(), 1, 10, 77)
	if err != nil {
		t.Fatal(err)
	}
	if len(got.Entries) != 1 || got.Entries[0].Games != 0 || got.Entries[0].RatingCenti != 150000 || got.Self.Rank != 1 {
		t.Fatalf("缺版本时应照常读出名次与分数、games=0,得到 %+v", got)
	}
}

func TestBoardSize(t *testing.T) {
	b, _ := newTestBoard(t)
	seedPlayers(t, b, 7)
	n, err := b.Size(context.Background())
	if err != nil || n != 7 {
		t.Fatalf("Size 应为 7,得到 %d %v", n, err)
	}
}
