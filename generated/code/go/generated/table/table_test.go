package table

import (
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

// configDir returns the absolute path to the JSON config directory.
func configDir(t *testing.T) string {
	t.Helper()
	// Walk up from the test file to find the generated/tables directory.
	_, filename, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("cannot determine test file path")
	}
	// test file is at generated/code/go/generated/table/table_test.go
	// JSON dir is at generated/tables/
	dir := filepath.Join(filepath.Dir(filename), "..", "..", "..", "..", "tables")
	abs, err := filepath.Abs(dir)
	if err != nil {
		t.Fatalf("cannot resolve config dir: %v", err)
	}
	if _, err := os.Stat(abs); err != nil {
		t.Fatalf("config dir not found: %s", abs)
	}
	return abs
}

// ---------------------------------------------------------------------------
// Load + Iterate
// ---------------------------------------------------------------------------

func TestLoadBuffTable(t *testing.T) {
	dir := configDir(t)
	mgr := NewBuffTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatalf("Load failed: %v", err)
	}
	if len(mgr.data) == 0 {
		t.Fatal("expected at least one Buff row")
	}
	for _, row := range mgr.data {
		t.Logf("Buff id=%d buff_type=%d level=%d", row.Id, row.BuffType, row.Level)
	}
}

func TestLoadTestMultiKeyTable(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatalf("Load failed: %v", err)
	}
	if len(mgr.data) == 0 {
		t.Fatal("expected at least one TestMultiKey row")
	}
}

// ---------------------------------------------------------------------------
// Key lookup
// ---------------------------------------------------------------------------

func TestMultiKeyGetByStringKey(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}
	row, ok := mgr.GetByString_key("aa")
	if !ok {
		t.Fatal("expected to find row by string_key 'aa'")
	}
	if row.StringKey != "aa" {
		t.Errorf("expected string_key='aa', got %q", row.StringKey)
	}
}

func TestMultiKeyGetByUint32Key(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}
	row, ok := mgr.GetByUint32_key(14)
	if !ok {
		t.Fatal("expected to find row by uint32_key 14")
	}
	if row.Id != 1 {
		t.Errorf("expected id=1, got %d", row.Id)
	}
}

func TestMultiKeyGetByInt32Key(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}
	row, ok := mgr.GetByInt32_key(8)
	if !ok {
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
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}

	// Verify every row's effect values appear in the index.
	for _, row := range mgr.data {
		for _, val := range row.Effect {
			rows := mgr.GetByEffectIndex(val)
			found := false
			for _, r := range rows {
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
	dir := configDir(t)
	mgr := NewBuffTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}

	for _, row := range mgr.data {
		for _, val := range row.SubBuff {
			rows := mgr.GetBySub_buffIndex(val)
			found := false
			for _, r := range rows {
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
// Per-column component structs
// ---------------------------------------------------------------------------

func TestScalarCompFromRow(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}
	row, ok := mgr.GetById(1)
	if !ok {
		t.Fatal("row id=1 not found")
	}

	idComp := MakeTestMultiKeyIdComp(row)
	if idComp.Value != 1 {
		t.Errorf("expected id comp value=1, got %d", idComp.Value)
	}

	strComp := MakeTestMultiKeyString_keyComp(row)
	if strComp.Value != "aa" {
		t.Errorf("expected string_key comp value='aa', got %q", strComp.Value)
	}

	u32Comp := MakeTestMultiKeyUint32_keyComp(row)
	if u32Comp.Value != 14 {
		t.Errorf("expected uint32_key comp value=14, got %d", u32Comp.Value)
	}

	i32Comp := MakeTestMultiKeyInt32_keyComp(row)
	if i32Comp.Value != 8 {
		t.Errorf("expected int32_key comp value=8, got %d", i32Comp.Value)
	}
}

func TestRepeatedCompSlice(t *testing.T) {
	dir := configDir(t)
	mgr := NewTestMultiKeyTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}

	// Row 2 has effect: [1, 1]
	row, ok := mgr.GetById(2)
	if !ok {
		t.Fatal("row id=2 not found")
	}

	effectComp := MakeTestMultiKeyEffectComp(row)
	if len(effectComp.Values) != len(row.Effect) {
		t.Errorf("effect comp length mismatch: got %d, want %d",
			len(effectComp.Values), len(row.Effect))
	}
	for i, v := range effectComp.Values {
		if v != row.Effect[i] {
			t.Errorf("effect[%d]: got %d, want %d", i, v, row.Effect[i])
		}
	}
}

func TestBuffCompStringField(t *testing.T) {
	dir := configDir(t)
	mgr := NewBuffTableManager()
	if err := mgr.Load(dir); err != nil {
		t.Fatal(err)
	}

	row, ok := mgr.GetById(1)
	if !ok {
		t.Fatal("buff row id=1 not found")
	}

	comp := MakeBuffHealth_regenerationComp(row)
	if comp.Value != row.HealthRegeneration {
		t.Errorf("expected %q, got %q", row.HealthRegeneration, comp.Value)
	}
}

// ---------------------------------------------------------------------------
// ReloadTables (hot-reload safety)
// ---------------------------------------------------------------------------

func TestReloadTables(t *testing.T) {
	dir := configDir(t)

	// Initial load via global instances.
	LoadTables(dir)

	row1, ok := TestMultiKeyTableManagerInstance.GetById(1)
	if !ok {
		t.Fatal("row id=1 not found after initial load")
	}
	if row1.StringKey != "aa" {
		t.Errorf("expected string_key='aa', got %q", row1.StringKey)
	}

	// Reload — should create fresh managers and swap.
	if err := ReloadTables(dir); err != nil {
		t.Fatalf("ReloadTables failed: %v", err)
	}

	// Old row1 should still be readable (old manager data not overwritten).
	if row1.StringKey != "aa" {
		t.Error("old row1 data was corrupted after reload")
	}

	// New instance should also have the data.
	row1New, ok := TestMultiKeyTableManagerInstance.GetById(1)
	if !ok {
		t.Fatal("row id=1 not found after reload")
	}
	if row1New.StringKey != "aa" {
		t.Errorf("expected string_key='aa' after reload, got %q", row1New.StringKey)
	}
}
