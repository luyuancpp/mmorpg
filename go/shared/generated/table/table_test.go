package table

import (
	"os"
	"path/filepath"
	"testing"

	pb "shared/generated/pb/table"
)

// configDir resolves the path to generated/tables/ relative to the repo root.
func configDir(t *testing.T) string {
	t.Helper()

	// Walk up from the test file location to find the repo root.
	// Expected: go/shared/generated/table/ -> repo root is 4 levels up.
	candidates := []string{
		filepath.Join("..", "..", "..", "..", "generated", "tables"),
		filepath.Join("..", "..", "..", "..", "..", "generated", "tables"),
	}

	// Also try from working directory (often repo root when run via `go test`).
	wd, _ := os.Getwd()
	if wd != "" {
		candidates = append(candidates, filepath.Join(wd, "generated", "tables"))
		candidates = append(candidates, filepath.Join(wd, "..", "..", "..", "..", "generated", "tables"))
	}

	for _, dir := range candidates {
		abs, err := filepath.Abs(dir)
		if err != nil {
			continue
		}
		if info, err := os.Stat(abs); err == nil && info.IsDir() {
			return abs
		}
	}
	t.Fatalf("config dir not found, tried: %v", candidates)
	return ""
}

func loadAllTables(t *testing.T) {
	t.Helper()
	dir := configDir(t)
	LoadTables(dir, false)
}

// 现有配表夹具中每个 id 恰好一行；管理器的正式契约允许主键重复。
// 组件与重载测试先校验夹具形状，再取行，不能把多值接口当成单值接口。
func singleMultiKeyFixtureRow(t *testing.T, mgr *TestMultiKeyTableManager, id uint32) *pb.TestMultiKeyTable {
	t.Helper()
	rows := mgr.FindAllById(id)
	if len(rows) != 1 || rows[0] == nil {
		t.Fatalf("expected exactly one non-nil fixture row for id=%d, got %v", id, rows)
	}
	return rows[0]
}

// ---------------------------------------------------------------------------
// Load + Iterate
// ---------------------------------------------------------------------------

func TestLoadAndIterateBuffTable(t *testing.T) {
	loadAllTables(t)
	mgr := BuffTableManagerInstance
	rows := mgr.FindAll()
	if len(rows) == 0 {
		t.Fatal("expected at least one Buff row")
	}
	for _, row := range rows {
		t.Logf("Buff id=%d buff_type=%d level=%d", row.Id, row.BuffType, row.Level)
	}
}

func TestLoadAndIterateTestMultiKeyTable(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	rows := mgr.FindAll()
	if len(rows) == 0 {
		t.Fatal("expected at least one TestMultiKey row")
	}
}

// ---------------------------------------------------------------------------
// Key lookup
// ---------------------------------------------------------------------------

func TestMultiKeyFindByStringKey(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	row, ok := mgr.FindByStringKey("aa")
	if !ok || row == nil {
		t.Fatal("expected to find row by string_key 'aa'")
	}
	if row.StringKey != "aa" {
		t.Errorf("expected StringKey='aa', got %q", row.StringKey)
	}
}

func TestMultiKeyFindByUint32Key(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	row, ok := mgr.FindByUint32Key(14)
	if !ok || row == nil {
		t.Fatal("expected to find row by uint32_key 14")
	}
	if row.Id != 1 {
		t.Errorf("expected id=1, got %d", row.Id)
	}
}

func TestMultiKeyFindByInt32Key(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	row, ok := mgr.FindByInt32Key(8)
	if !ok || row == nil {
		t.Fatal("expected to find row by int32_key 8")
	}
	if row.Id != 1 {
		t.Errorf("expected id=1, got %d", row.Id)
	}
}

// ---------------------------------------------------------------------------
// Repeated column value index
// ---------------------------------------------------------------------------

func TestRepeatedEffectIndex(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	for _, row := range mgr.FindAll() {
		for _, val := range row.Effect {
			indexed := mgr.FindByEffectIndex(val)
			found := false
			for _, r := range indexed {
				if r.Id == row.Id {
					found = true
					break
				}
			}
			if !found {
				t.Errorf("effect value %d not indexed for row id %d", val, row.Id)
			}
		}
	}
}

func TestBuffSubBuffIndex(t *testing.T) {
	loadAllTables(t)
	mgr := BuffTableManagerInstance
	for _, row := range mgr.FindAll() {
		for _, val := range row.SubBuff {
			indexed := mgr.FindBySubBuffIndex(val)
			found := false
			for _, r := range indexed {
				if r.Id == row.Id {
					found = true
					break
				}
			}
			if !found {
				t.Errorf("sub_buff value %d not indexed for row id %d", val, row.Id)
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Per-column component records
// ---------------------------------------------------------------------------

func TestScalarCompFromRow(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	row := singleMultiKeyFixtureRow(t, mgr, 1)

	idComp := MakeTestMultiKeyIdComp(row)
	if idComp.Value != 1 {
		t.Errorf("expected Id=1, got %d", idComp.Value)
	}

	strComp := MakeTestMultiKeyString_keyComp(row)
	if strComp.Value != "aa" {
		t.Errorf("expected StringKey='aa', got %q", strComp.Value)
	}

	u32Comp := MakeTestMultiKeyUint32_keyComp(row)
	if u32Comp.Value != 14 {
		t.Errorf("expected Uint32Key=14, got %d", u32Comp.Value)
	}

	i32Comp := MakeTestMultiKeyInt32_keyComp(row)
	if i32Comp.Value != 8 {
		t.Errorf("expected Int32Key=8, got %d", i32Comp.Value)
	}
}

func TestRepeatedCompSlice(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	row := singleMultiKeyFixtureRow(t, mgr, 2)

	effectComp := MakeTestMultiKeyEffectComp(row)
	if len(effectComp.Values) != len(row.Effect) {
		t.Errorf("expected %d effect values, got %d", len(row.Effect), len(effectComp.Values))
	}
	for i, v := range effectComp.Values {
		if v != row.Effect[i] {
			t.Errorf("effect[%d]: expected %d, got %d", i, row.Effect[i], v)
		}
	}
}

// ---------------------------------------------------------------------------
// Reload (hot-reload safety: snapshot swap, no accumulation)
// ---------------------------------------------------------------------------

func TestReloadDoesNotAccumulateData(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance
	countBefore := mgr.Count()
	if countBefore == 0 {
		t.Fatal("expected at least one row")
	}

	// Reload the same data.
	dir := configDir(t)
	if err := mgr.Load(dir, false); err != nil {
		t.Fatalf("reload failed: %v", err)
	}

	// Count must stay the same — old snapshot replaced, not accumulated.
	if mgr.Count() != countBefore {
		t.Errorf("expected count=%d after reload, got %d", countBefore, mgr.Count())
	}

	// Data should still be accessible.
	row := singleMultiKeyFixtureRow(t, mgr, 1)
	if row.Id != 1 {
		t.Errorf("expected id=1, got %d", row.Id)
	}
}

func TestReloadReplacesOldPointers(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance

	rowBefore := singleMultiKeyFixtureRow(t, mgr, 1)

	dir := configDir(t)
	if err := mgr.Load(dir, false); err != nil {
		t.Fatalf("reload failed: %v", err)
	}

	rowAfter := singleMultiKeyFixtureRow(t, mgr, 1)

	if rowBefore == rowAfter {
		t.Error("reload should produce a new snapshot (different pointers)")
	}
	if rowAfter.Id != 1 {
		t.Errorf("expected id=1, got %d", rowAfter.Id)
	}
}

func TestReloadMultiKeyIndicesConsistent(t *testing.T) {
	loadAllTables(t)
	mgr := TestMultiKeyTableManagerInstance

	// Reload twice to stress test.
	dir := configDir(t)
	_ = mgr.Load(dir, false)
	_ = mgr.Load(dir, false)

	// Multi-key lookup should still work.
	rows := mgr.FindByMUint32Key(17)
	if len(rows) == 0 {
		t.Log("no rows with m_uint32_key=17 (may be expected depending on data)")
	}

	// Count should be stable.
	countBefore := mgr.Count()
	_ = mgr.Load(dir, false)
	if mgr.Count() != countBefore {
		t.Errorf("count changed after 3rd reload: %d -> %d", countBefore, mgr.Count())
	}
}

// 可重复主键必须返回全部匹配行，缺失主键必须返回空结果。
func TestMultiKeyFindAllByIdReturnsAllRows(t *testing.T) {
	dir := t.TempDir()
	fixture := `{"data":[
 {"id":7,"string_key":"first","uint32_key":101,"int32_key":1},
 {"id":7,"string_key":"second","uint32_key":102,"int32_key":2},
 {"id":8,"string_key":"third","uint32_key":103,"int32_key":3}
]}`
	if err := os.WriteFile(filepath.Join(dir, "testmultikey.json"), []byte(fixture), 0o600); err != nil {
		t.Fatal(err)
	}
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir, false); err != nil {
		t.Fatalf("load duplicate-id fixture: %v", err)
	}
	rows := mgr.FindAllById(7)
	if len(rows) != 2 {
		t.Fatalf("expected both rows for id=7, got %d", len(rows))
	}
	if rows[0].Id != 7 || rows[1].Id != 7 || rows[0].StringKey != "first" || rows[1].StringKey != "second" {
		t.Fatalf("duplicate-id rows must preserve table order, got %v", rows)
	}
	other := mgr.FindAllById(8)
	if len(other) != 1 || other[0].StringKey != "third" {
		t.Fatalf("expected only the row for id=8, got %v", other)
	}
	if missing := mgr.FindAllById(99); len(missing) != 0 {
		t.Fatalf("expected no rows for missing id=99, got %v", missing)
	}
}
