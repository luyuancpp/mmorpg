package loginqueue

import (
	mathrand "math/rand/v2"
	"slices"
	"testing"
)

// seqIntn 按给定序列依次返回随机数,把 pickTwoChoices 的两次抽样钉死。序列用完或越界即判测试失败。
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

// 抽中的两台里取人数少的,同人数取第一次抽中的;第二次抽样在去掉第一台后的 n-1 台里取,两台一定不同。
func TestPickTwoChoices_PicksLighterOfSampledPair(t *testing.T) {
	cands := []GateCandidate{
		{NodeID: 7, PlayerCount: 2},
		{NodeID: 9, PlayerCount: 1},
		{NodeID: 2, PlayerCount: 3},
		{NodeID: 4, PlayerCount: 1},
	}
	cases := []struct {
		name string
		seq  []int
		want uint32
	}{
		// i=0(7号,2 人);j=0 ≥ i 顺延到 1(9号,1 人)→ 9 号。
		{"second draw skips the first", []int{0, 0}, 9},
		// i=2(2号,3 人);j=2 顺延到 3(4号,1 人)→ 4 号。
		{"busiest loses to the other", []int{2, 2}, 4},
		// i=1(9号,1 人);j=2 顺延到 3(4号,1 人):同人数取第一次抽中的 → 9 号(不看 NodeID)。
		{"tie keeps the first draw", []int{1, 2}, 9},
		// 反过来抽:i=3(4号);j=1(9号):同人数仍取第一次抽中的 → 4 号。
		{"tie keeps the first draw reversed", []int{3, 1}, 4},
		// i=3(4号,1 人);j=0(7号,2 人)→ 4 号。
		{"first draw can win", []int{3, 0}, 4},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got, err := pickTwoChoices(cands, seqIntn(t, tc.seq...))
			if err != nil || got.NodeID != tc.want {
				t.Fatalf("pick = (%+v, %v), want NodeID %d", got, err, tc.want)
			}
		})
	}
}

// 最忙的那台不可能被选中(没有和它同样忙的 gate 时):枚举全部抽样组合验证。
func TestPickTwoChoices_NeverPicksStrictlyBusiest(t *testing.T) {
	cands := []GateCandidate{
		{NodeID: 1, PlayerCount: 10},
		{NodeID: 2, PlayerCount: 500},
		{NodeID: 3, PlayerCount: 20},
		{NodeID: 4, PlayerCount: 30},
	}
	for i := range len(cands) {
		for j := range len(cands) - 1 {
			got, err := pickTwoChoices(cands, seqIntn(t, i, j))
			if err != nil {
				t.Fatalf("draws (%d,%d): %v", i, j, err)
			}
			if got.NodeID == 2 {
				t.Fatalf("draws (%d,%d) picked the strictly busiest gate", i, j)
			}
		}
	}
}

// 全部同人数(开新区第一个上报窗口,所有 gate 都报 0):枚举全部抽样组合,每台被选中的次数恰好相同。
// 同人数若按 NodeID 定胜负,NodeID 最大的那台一次都选不中(评审 B1)。
func TestPickTwoChoices_EqualLoadIsUniform(t *testing.T) {
	cands := []GateCandidate{{NodeID: 40}, {NodeID: 10}, {NodeID: 30}, {NodeID: 20}}
	counts := make(map[uint32]int)
	for i := range len(cands) {
		for j := range len(cands) - 1 {
			got, err := pickTwoChoices(cands, seqIntn(t, i, j))
			if err != nil {
				t.Fatalf("draws (%d,%d): %v", i, j, err)
			}
			counts[got.NodeID]++
		}
	}
	for _, c := range cands {
		if counts[c.NodeID] != len(cands)-1 {
			t.Fatalf("picks per gate = %v, want each gate exactly %d times", counts, len(cands)-1)
		}
	}
}

// 边界:空候选报错;一台时不抽样直接返回;两台时无论怎么抽都是负载少的那台。不改调用方的切片。
func TestPickTwoChoices_Boundaries(t *testing.T) {
	if got, err := pickTwoChoices(nil, seqIntn(t)); got != nil || err == nil {
		t.Fatalf("empty candidates = (%v, %v), want (nil, error)", got, err)
	}
	if got, err := PickGate(nil); got != nil || err == nil {
		t.Fatalf("PickGate(nil) = (%v, %v), want (nil, error)", got, err)
	}

	one := []GateCandidate{{NodeID: 5, PlayerCount: 99}}
	if got, err := pickTwoChoices(one, seqIntn(t)); err != nil || got.NodeID != 5 {
		t.Fatalf("single candidate = (%+v, %v), want NodeID 5 without drawing", got, err)
	}

	two := []GateCandidate{{NodeID: 8, PlayerCount: 7}, {NodeID: 3, PlayerCount: 2}}
	orig := slices.Clone(two)
	for _, seq := range [][]int{{0, 0}, {1, 0}} {
		if got, err := pickTwoChoices(two, seqIntn(t, seq...)); err != nil || got.NodeID != 3 {
			t.Fatalf("two candidates draws %v = (%+v, %v), want the lighter NodeID 3", seq, got, err)
		}
	}
	got, err := PickGate(two)
	if err != nil || got.NodeID != 3 {
		t.Fatalf("PickGate(two) = (%+v, %v), want NodeID 3", got, err)
	}
	got.PlayerCount = 12345 // 返回的是副本,改它不影响候选
	if !slices.Equal(two, orig) {
		t.Fatalf("pick mutated its input: %v, want %v", two, orig)
	}
}

// 羊群回归(容量模型 A0-7):同一份滞后快照上连挑多次,旧规则(确定性取最少)会全部落到最闲的那台。
// 随机二选一下,最闲那台只在被抽中时才赢:10 台时概率 1 - C(9,2)/C(10,2) = 20%。
// 随机源用固定种子,结果确定、可重复。
func TestPickTwoChoices_SpreadsOnStaleSnapshot(t *testing.T) {
	cands := make([]GateCandidate, 10)
	for i := range cands {
		cands[i] = GateCandidate{NodeID: uint32(i + 1), PlayerCount: 100}
	}
	cands[0].PlayerCount = 0 // 最闲的一台

	rng := mathrand.New(mathrand.NewPCG(20261018, 1))
	const picks = 2000
	counts := make(map[uint32]int)
	for range picks {
		got, err := pickTwoChoices(cands, rng.IntN)
		if err != nil {
			t.Fatal(err)
		}
		counts[got.NodeID]++
	}
	if share := float64(counts[1]) / picks; share < 0.15 || share > 0.25 {
		t.Fatalf("lightest gate share = %.3f, want about 0.20 (two random choices out of 10)", share)
	}
	if len(counts) != len(cands) {
		t.Fatalf("picks reached %d of %d gates, want all of them: %v", len(counts), len(cands), counts)
	}
}

// PickGate 真的接了随机源:三台里两台非最忙的都会被选到。若误接成常量源(如 func(int) int { return 0 }),
// 每次都是 i=0、j 顺延到 1,固定选同一台。真随机下 200 次一次都抽不到另一台的概率是 (2/3)^200 ≈ 1e-35,实际上是确定的。
func TestPickGate_UsesARandomSource(t *testing.T) {
	cands := []GateCandidate{{NodeID: 1, PlayerCount: 30}, {NodeID: 2, PlayerCount: 10}, {NodeID: 3, PlayerCount: 20}}
	seen := make(map[uint32]int)
	for range 200 {
		got, err := PickGate(cands)
		if err != nil {
			t.Fatal(err)
		}
		seen[got.NodeID]++
	}
	if len(seen) != 2 || seen[2] == 0 || seen[3] == 0 {
		t.Fatalf("picks = %v, want both non-busiest gates 2 and 3 and never the busiest 1", seen)
	}
}
