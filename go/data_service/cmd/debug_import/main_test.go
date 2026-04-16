package main

import (
	"encoding/base64"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"

	"data_service/cmd/debugutil"
)

func TestLoadExportFilePlayerMode(t *testing.T) {
	content := `{
  "mode": "player",
  "player_id": 12345,
  "home_zone_id": 1,
  "version": 42,
  "field_count": 2,
  "fields": {
    "bag": {
      "kind": "protobuf",
      "size": 10,
      "raw_base64": "` + base64.StdEncoding.EncodeToString([]byte("bag-binary")) + `",
      "proto_type": "PlayerDatabase"
    },
    "currency": {
      "kind": "text",
      "size": 5,
      "raw_base64": "` + base64.StdEncoding.EncodeToString([]byte("hello")) + `",
      "text": "hello"
    }
  }
}`
	path := filepath.Join(t.TempDir(), "export.json")
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}

	data, err := loadExportFile(path)
	if err != nil {
		t.Fatalf("loadExportFile: %v", err)
	}
	if data.Mode != "player" {
		t.Fatalf("expected mode=player, got %q", data.Mode)
	}
	if data.PlayerID != 12345 {
		t.Fatalf("expected player_id=12345, got %d", data.PlayerID)
	}
	if data.HomeZoneID != 1 {
		t.Fatalf("expected home_zone_id=1, got %d", data.HomeZoneID)
	}
	if len(data.Fields) != 2 {
		t.Fatalf("expected 2 fields, got %d", len(data.Fields))
	}

	bag := data.Fields["bag"]
	if bag.RawBase64 == "" {
		t.Fatal("expected bag.RawBase64 to be set")
	}
	decoded, _ := base64.StdEncoding.DecodeString(bag.RawBase64)
	if string(decoded) != "bag-binary" {
		t.Fatalf("expected bag raw bytes = 'bag-binary', got %q", decoded)
	}
}

func TestLoadExportFileMissingMode(t *testing.T) {
	path := filepath.Join(t.TempDir(), "bad.json")
	if err := os.WriteFile(path, []byte(`{"player_id": 1}`), 0o644); err != nil {
		t.Fatal(err)
	}

	_, err := loadExportFile(path)
	if err == nil {
		t.Fatal("expected error for missing mode")
	}
}

func TestResolveColumnValueNullAndScalar(t *testing.T) {
	// null
	v, err := resolveColumnValue(json.RawMessage(`null`))
	if err != nil || v != nil {
		t.Fatalf("null: got %v, err %v", v, err)
	}

	// string
	v, err = resolveColumnValue(json.RawMessage(`"hello"`))
	if err != nil || v != "hello" {
		t.Fatalf("string: got %v, err %v", v, err)
	}

	// integer
	v, err = resolveColumnValue(json.RawMessage(`42`))
	if err != nil {
		t.Fatalf("int: err %v", err)
	}
	if v != int64(42) {
		t.Fatalf("int: got %v (%T)", v, v)
	}

	// float
	v, err = resolveColumnValue(json.RawMessage(`3.14`))
	if err != nil {
		t.Fatalf("float: err %v", err)
	}
	if v != 3.14 {
		t.Fatalf("float: got %v", v)
	}
}

func TestResolveColumnValueRenderedBinary(t *testing.T) {
	raw := []byte{0x08, 0x96, 0x01}
	b64 := base64.StdEncoding.EncodeToString(raw)

	rv := map[string]any{
		"kind":       "base64",
		"size":       3,
		"raw_base64": b64,
	}
	encoded, _ := json.Marshal(rv)

	v, err := resolveColumnValue(json.RawMessage(encoded))
	if err != nil {
		t.Fatalf("resolveColumnValue: %v", err)
	}

	got, ok := v.([]byte)
	if !ok {
		t.Fatalf("expected []byte, got %T", v)
	}
	if len(got) != len(raw) || got[0] != raw[0] || got[1] != raw[1] || got[2] != raw[2] {
		t.Fatalf("bytes mismatch: got %v, want %v", got, raw)
	}
}

func TestResolveColumnValueRenderedText(t *testing.T) {
	rv := map[string]any{
		"kind": "text",
		"size": 5,
		"text": "hello",
	}
	encoded, _ := json.Marshal(rv)

	v, err := resolveColumnValue(json.RawMessage(encoded))
	if err != nil {
		t.Fatalf("resolveColumnValue: %v", err)
	}
	if v != "hello" {
		t.Fatalf("expected 'hello', got %v", v)
	}
}

func TestQuoteIdentifierSafety(t *testing.T) {
	good := []string{"player_table", "data", "col1"}
	for _, name := range good {
		if _, err := debugutil.QuoteIdentifier(name); err != nil {
			t.Fatalf("expected %q to pass: %v", name, err)
		}
	}

	bad := []string{"", "player-table", "t;drop", "a b"}
	for _, name := range bad {
		if _, err := debugutil.QuoteIdentifier(name); err == nil {
			t.Fatalf("expected %q to be rejected", name)
		}
	}
}
