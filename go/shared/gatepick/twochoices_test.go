package gatepick

import (
	mathrand "math/rand/v2"
	"testing"
)

type gate struct {
	id   uint32
	load uint32
}

func gateLoad(g gate) uint32 { return g.load }

// seqIntn 按给定序列依次返回随机数,把两次抽样钉死。序列用完或越界即判测试失败。
func seqIntn(t *testing.T, seq ...int) func(int) int {
	t.Helper()
	next := 0
	return func(n int) int {
		t.Helper()
		if next >= len(seq) {
			t.Fatalf("intn called %d times, sequence only has %d values", next+1, len(seq))
		}
		v := seq[next]
		next++
		if v < 0 || v >= n {
			t.Fatalf("sequence value %d out of range [0,%d)", v, n)
		}
		return v
	}
}

func pick(t *testing.T, items []gate, seq ...int) gate {
	t.Helper()
	i, ok := TwoChoices(items, gateLoad, seqIntn(t, seq...))
	if !ok {
		t.Fatalf("TwoChoices(%v) = not ok", items)
	}
	return items[i]
}

// 抽中的两个里取 load 小的,同 load 取第一次抽中的;第二次抽样在去掉第一个后的 n-1 个里取。
func TestTwoChoices_PicksLighterOfSampledPair(t *testing.T) {
	items := []gate{{7, 2}, {9, 1}, {2, 3}, {4, 1}}
	cases := []struct {
		name string
		seq  []int
		want uint32
	}{
		// i=0(2);j=0 顺延到 1(1)。
		{"second draw skips the first", []int{0, 0}, 9},
		// i=2(3);j=2 顺延到 3(1)。
		{"busiest loses", []int{2, 2}, 4},
		// i=1(1);j=2 顺延到 3(1):同 load 取第一次抽中的 i。
		{"tie keeps the first draw", []int{1, 2}, 9},
		// i=3(1);j=1(1):同 load 取 i。
		{"tie keeps the first draw reversed", []int{3, 1}, 4},
		// i=3(1);j=0(2)。
		{"first draw can win", []int{3, 0}, 4},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := pick(t, items, tc.seq...); got.id != tc.want {
				t.Fatalf("pick = %d, want %d", got.id, tc.want)
			}
		})
	}
}

// 严格最忙的那个在任何抽样组合下都选不中。
func TestTwoChoices_NeverPicksStrictlyBusiest(t *testing.T) {
	items := []gate{{1, 10}, {2, 500}, {3, 20}, {4, 30}}
	for i := range len(items) {
		for j := range len(items) - 1 {
			if got := pick(t, items, i, j); got.id == 2 {
				t.Fatalf("draws (%d,%d) picked the strictly busiest item", i, j)
			}
		}
	}
}

// 全部同 load(开新区第一个上报窗口):枚举全部抽样组合,每个被选中的次数恰好相同。
func TestTwoChoices_EqualLoadIsUniform(t *testing.T) {
	items := []gate{{40, 0}, {10, 0}, {30, 0}, {20, 0}}
	counts := make(map[uint32]int)
	for i := range len(items) {
		for j := range len(items) - 1 {
			counts[pick(t, items, i, j).id]++
		}
	}
	for _, g := range items {
		if counts[g.id] != len(items)-1 {
			t.Fatalf("picks per item = %v, want each exactly %d", counts, len(items)-1)
		}
	}
}

// 边界:空集合 ok=false 且不抽样;一个时不抽样直接返回;两个时无论怎么抽都是 load 小的。
func TestTwoChoices_Boundaries(t *testing.T) {
	if _, ok := TwoChoices([]gate(nil), gateLoad, seqIntn(t)); ok {
		t.Fatal("empty items must not be ok")
	}
	if got := pick(t, []gate{{5, 99}}); got.id != 5 {
		t.Fatalf("single item = %d, want 5 without drawing", got.id)
	}
	two := []gate{{8, 7}, {3, 2}}
	for _, seq := range [][]int{{0, 0}, {1, 0}} {
		if got := pick(t, two, seq...); got.id != 3 {
			t.Fatalf("two items draws %v = %d, want the lighter 3", seq, got.id)
		}
	}
}

// 羊群回归:同一份滞后快照上连挑多次,最闲那台只在被抽中时才赢,10 个时概率 1 - C(9,2)/C(10,2) = 20%;
// 其余同 load 的都能被选到。随机源用固定种子,结果确定、可重复。
func TestTwoChoices_SpreadsOnStaleSnapshot(t *testing.T) {
	items := make([]gate, 10)
	for i := range items {
		items[i] = gate{id: uint32(i + 1), load: 100}
	}
	items[0].load = 0

	rng := mathrand.New(mathrand.NewPCG(20261018, 1))
	const picks = 2000
	counts := make(map[uint32]int)
	for range picks {
		i, ok := TwoChoices(items, gateLoad, rng.IntN)
		if !ok {
			t.Fatal("not ok")
		}
		counts[items[i].id]++
	}
	if share := float64(counts[1]) / picks; share < 0.15 || share > 0.25 {
		t.Fatalf("lightest share = %.3f, want about 0.20", share)
	}
	if len(counts) != len(items) {
		t.Fatalf("picks reached %d of %d items, want all: %v", len(counts), len(items), counts)
	}
}
