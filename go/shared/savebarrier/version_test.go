package savebarrier

import (
	"google.golang.org/protobuf/proto"
	"math"
	databasepb "proto/common/database"
	"testing"
)

func TestInitialPlayerCommitIsCompleteAndNotAnActor(t *testing.T) {
	parent := InitialPlayerData(42)
	if !IsInitialPlayerData(parent) || parent.LastSaveSeq != 1 ||
		RowVersion(parent.PlayerDatabaseData.FenceOwnerEpoch, parent.PlayerDatabaseData.FenceSaveSeq) != Version(1, 1) ||
		RowVersion(parent.PlayerDatabase_1Data.FenceOwnerEpoch, parent.PlayerDatabase_1Data.FenceSaveSeq) != Version(1, 1) {
		t.Fatal("初始持久提交没有完整覆盖父水位和两张 SQL 围栏")
	}
	if IsInitialPlayerData(InitialPlayerData(0)) {
		t.Fatal("接受了零玩家初始提交")
	}
	changed := proto.Clone(parent).(*databasepb.PlayerAllData)
	changed.LastSaveSeq = 0
	if IsInitialPlayerData(changed) {
		t.Fatal("接受了零水位缓存种子")
	}
	changed = proto.Clone(parent).(*databasepb.PlayerAllData)
	changed.ProtoReflect().SetUnknown([]byte{0xA0, 0x06, 0x01})
	if IsInitialPlayerData(changed) {
		t.Fatal("初始凭据丢弃了未知业务字段")
	}
}

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
