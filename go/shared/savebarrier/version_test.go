package savebarrier

import (
	"math"
	"testing"
)

func TestVersionRoundTripAndOrdering(t *testing.T) {
	for _, tuple := range [][2]uint64{{0, 0}, {1, 9}, {1, 10}, {2, 0}, {math.MaxUint64, math.MaxUint64}} {
		v := Version(tuple[0], tuple[1])
		epoch, seq, err := Parse(v)
		if err != nil || epoch != tuple[0] || seq != tuple[1] {
			t.Fatalf("%v -> %q -> %d/%d: %v", tuple, v, epoch, seq, err)
		}
	}
	if Version(1, 9) >= Version(1, 10) || Version(1, math.MaxUint64) >= Version(2, 0) {
		t.Fatal("定宽版本顺序不等于整数元组顺序")
	}
}

func TestParseRejectsMalformedAndOverflow(t *testing.T) {
	for _, v := range []string{"", "1:2", "00000000000000000001/00000000000000000002", "0000000000000000000x:00000000000000000002", "18446744073709551616:00000000000000000001", "00000000000000000001:18446744073709551616"} {
		if _, _, err := Parse(v); err == nil {
			t.Fatalf("接受了畸形版本 %q", v)
		}
	}
	if Key(42) != "player:{42}:save_barrier" {
		t.Fatal("屏障键未保留玩家 hash tag")
	}
}
