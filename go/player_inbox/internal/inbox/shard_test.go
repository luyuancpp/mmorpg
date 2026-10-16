package inbox

import "testing"

// TestRowShard 钉住 shard = player_id % 64(建表契约,设计 §4.2)。
func TestRowShard(t *testing.T) {
	if ShardCount != 64 {
		t.Fatalf("ShardCount = %d;模数是建表契约,改它等于改全部存量行的主键", ShardCount)
	}
	cases := map[uint64]uint32{0: 0, 1: 1, 63: 63, 64: 0, 65: 1, 1<<63 + 5: 5, ^uint64(0): 63}
	for pid, want := range cases {
		if got := RowShard(pid); got != want {
			t.Errorf("RowShard(%d) = %d, want %d", pid, got, want)
		}
	}
}

// TestCheckRowShard:读出时断言 shard 与 player_id 一致(TiDB 不执行 CHECK)。
func TestCheckRowShard(t *testing.T) {
	if err := checkRowShard(RowShard(12345), 12345); err != nil {
		t.Fatalf("一致的行不应报错: %v", err)
	}
	if err := checkRowShard(RowShard(12345)+1, 12345); err == nil {
		t.Fatal("shard 与 player_id 不一致必须报错")
	}
}
