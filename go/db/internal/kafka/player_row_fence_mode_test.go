package kafka

import (
	"context"
	"testing"

	db_config "db/internal/config"

	playerpb "proto/common/database"
	dbpb "proto/db"
)

// 缺省 legacy:玩家表不进行内围栏,调用者照旧普通 Save;这是 C++ 回滚到旧版时不静默丢盘的保障。
// store 传 nil:legacy 必须在任何 SQL 之前返回。
func TestSavePlayerRowLegacyModeLeavesPlayerTablesToPlainSave(t *testing.T) {
	t.Cleanup(func() { _ = SetFencingMode(db_config.FencingModeLegacy) })
	for _, mode := range []string{"", db_config.FencingModeLegacy} {
		if err := SetFencingMode(mode); err != nil {
			t.Fatalf("mode %q: %v", mode, err)
		}
		handled, err := savePlayerRow(context.Background(), nil, &dbpb.DBTask{Key: 7, OwnerEpoch: 3, SaveSeq: 9}, &playerpb.PlayerDatabase{PlayerId: 7})
		if handled || err != nil {
			t.Fatalf("mode %q: player_database must fall back to plain Save, got handled=%v err=%v", mode, handled, err)
		}
		handled, err = savePlayerRow(context.Background(), nil, &dbpb.DBTask{Key: 7, OwnerEpoch: 3, SaveSeq: 9}, &playerpb.PlayerDatabase_1{PlayerId: 7})
		if handled || err != nil {
			t.Fatalf("mode %q: player_database_1 must fall back to plain Save, got handled=%v err=%v", mode, handled, err)
		}
	}
}

func TestSetFencingModeRejectsUnknownMode(t *testing.T) {
	t.Cleanup(func() { _ = SetFencingMode(db_config.FencingModeLegacy) })
	if err := SetFencingMode(db_config.FencingModeDual); err != nil {
		t.Fatal(err)
	}
	if err := SetFencingMode("Dual"); err == nil {
		t.Fatal("模式名大小写敏感,未知值必须拒绝")
	}
	if !fencingDual.Load() {
		t.Fatal("拒绝的设置不得改动已生效的模式")
	}
}
