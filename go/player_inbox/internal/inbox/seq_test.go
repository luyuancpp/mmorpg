package inbox

import (
	"errors"
	"testing"
)

const testNowMs = uint64(1_800_000_000_000)

// TestNextSeqTakesMaxOfCounterAndClockFloor 钉住 seq = max(next_seq, now_ms << 12)(IB-6)。
func TestNextSeqTakesMaxOfCounterAndClockFloor(t *testing.T) {
	floor := testNowMs << 12
	cases := []struct {
		name    string
		nextSeq uint64
		want    uint64
	}{
		{"新流:时间下界", 1, floor},
		{"计数器领先:取计数器", floor + 10, floor + 10},
		{"计数器恰好等于下界", floor, floor},
		{"next_seq 为 0 也不返回 0(用时间下界)", 0, floor},
	}
	for _, tc := range cases {
		got, err := NextSeq(tc.nextSeq, testNowMs)
		if err != nil {
			t.Fatalf("%s: %v", tc.name, err)
		}
		if got != tc.want {
			t.Errorf("%s: NextSeq(%d, %d) = %d, want %d", tc.name, tc.nextSeq, testNowMs, got, tc.want)
		}
	}
}

// TestNextSeqMonotonicUnderClockRollback:同一条流内,时钟回拨也保持严格递增(调用方写回 next_seq = seq + 1)。
func TestNextSeqMonotonicUnderClockRollback(t *testing.T) {
	clocks := []uint64{testNowMs, testNowMs, testNowMs - 60_000, testNowMs - 1, testNowMs + 1, testNowMs - 3_600_000}
	next := uint64(1)
	var prev uint64
	for i, now := range clocks {
		seq, err := NextSeq(next, now)
		if err != nil {
			t.Fatalf("step %d: %v", i, err)
		}
		if seq <= prev {
			t.Fatalf("step %d: seq %d 不大于上一个 %d(时钟 %d)", i, seq, prev, now)
		}
		prev, next = seq, seq+1
	}
}

// TestNextSeqSameMillisecondIsContiguous:同一毫秒内连续分配的号连续(并发入箱 seq 无空洞的前提)。
func TestNextSeqSameMillisecondIsContiguous(t *testing.T) {
	next := uint64(1)
	first, err := NextSeq(next, testNowMs)
	if err != nil {
		t.Fatal(err)
	}
	next = first + 1
	for i := uint64(1); i < 100; i++ {
		seq, err := NextSeq(next, testNowMs)
		if err != nil {
			t.Fatal(err)
		}
		if seq != first+i {
			t.Fatalf("第 %d 个号 %d,want %d", i, seq, first+i)
		}
		next = seq + 1
	}
}

// TestNextSeqRejectsUnusableClock:时钟为 0 或大到左移溢出时拒绝分配(按故障处理,不入箱)。
func TestNextSeqRejectsUnusableClock(t *testing.T) {
	for _, now := range []uint64{0, maxSeqClockMs, ^uint64(0)} {
		if _, err := NextSeq(1, now); !errors.Is(err, ErrSeqClockInvalid) {
			t.Errorf("NextSeq(1, %d) err = %v, want ErrSeqClockInvalid", now, err)
		}
	}
	if _, err := NextSeq(1, maxSeqClockMs-1); err != nil {
		t.Errorf("边界内的时钟应可用: %v", err)
	}
}

// TestRaiseNextSeqOnCursorAhead:消费方水位 ≥ next_seq(收件箱库被恢复过)时抬到水位 + 1,只升不降。
func TestRaiseNextSeqOnCursorAhead(t *testing.T) {
	cases := []struct {
		name          string
		nextSeq, dt   uint64
		want          uint64
		raised, ahead bool
	}{
		{"水位落后:不动", 100, 50, 100, false, false},
		{"水位刚好处理到 next_seq-1:不动", 100, 99, 100, false, false},
		{"水位等于 next_seq:抬到水位+1", 100, 100, 101, true, true},
		{"水位远超:抬到水位+1", 100, 5000, 5001, true, true},
		{"新流且消费方没有水位", 1, 0, 1, false, false},
	}
	for _, tc := range cases {
		got, raised := RaiseNextSeq(tc.nextSeq, tc.dt)
		if got != tc.want || raised != tc.raised {
			t.Errorf("%s: RaiseNextSeq(%d, %d) = (%d, %v), want (%d, %v)", tc.name, tc.nextSeq, tc.dt, got, raised, tc.want, tc.raised)
		}
		if IsCursorAhead(tc.nextSeq, tc.dt) != tc.ahead {
			t.Errorf("%s: IsCursorAhead(%d, %d) = %v, want %v", tc.name, tc.nextSeq, tc.dt, !tc.ahead, tc.ahead)
		}
	}
}

// TestCursorAheadThenAllocateStaysAboveWatermark:抬号之后即使时钟偏慢,新 seq 也在消费方水位之上(设计 §7.8 第 2 步)。
func TestCursorAheadThenAllocateStaysAboveWatermark(t *testing.T) {
	watermark := (testNowMs + 86_400_000) << 12 // 消费方水位来自「未来」:恢复后的墙钟比备份时还慢一天
	next, _ := RaiseNextSeq(1, watermark)
	seq, err := NextSeq(next, testNowMs)
	if err != nil {
		t.Fatal(err)
	}
	if seq <= watermark {
		t.Fatalf("抬号后新 seq %d 应大于消费方水位 %d", seq, watermark)
	}
}
