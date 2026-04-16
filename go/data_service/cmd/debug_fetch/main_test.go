package main

import (
	"testing"

	"data_service/cmd/debugutil"
	database "data_service/proto/common/database"

	"google.golang.org/protobuf/proto"
)

func TestEnsureReadOnlyQueryAllowsSafeStatements(t *testing.T) {
	tests := []string{
		"SELECT * FROM player LIMIT 10",
		" select player_id from account where player_id = 1 ; ",
		"SHOW TABLES",
		"DESCRIBE player_table",
		"EXPLAIN SELECT * FROM player_table WHERE player_id = 1",
	}

	for _, query := range tests {
		if err := ensureReadOnlyQuery(query); err != nil {
			t.Fatalf("expected query to be allowed: %q, got error: %v", query, err)
		}
	}
}

func TestEnsureReadOnlyQueryRejectsDangerousStatements(t *testing.T) {
	tests := []string{
		"",
		"DELETE FROM player",
		"UPDATE player SET level = 99",
		"INSERT INTO player(id) VALUES(1)",
		"DROP TABLE player",
		"SELECT * FROM player; DELETE FROM player",
	}

	for _, query := range tests {
		if err := ensureReadOnlyQuery(query); err == nil {
			t.Fatalf("expected query to be rejected: %q", query)
		}
	}
}

func TestRenderValueKind(t *testing.T) {
	jsonValue := []byte(`{"hp":123,"name":"hero"}`)
	decoded := renderValue(jsonValue)
	if decoded.Kind != "json" {
		t.Fatalf("expected json kind, got %q", decoded.Kind)
	}

	textValue := []byte("plain-text")
	decoded = renderValue(textValue)
	if decoded.Kind != "text" {
		t.Fatalf("expected text kind, got %q", decoded.Kind)
	}

	binaryValue := []byte{0x08, 0x96, 0x01}
	decoded = renderValue(binaryValue)
	if decoded.Kind != "binary" {
		t.Fatalf("expected binary kind, got %q", decoded.Kind)
	}
}

func TestResolveModeNameAliases(t *testing.T) {
	cases := map[string]string{
		"zone":      "zone",
		"server":    "zone",
		"player-db": "playerdb",
		"player_db": "playerdb",
	}

	for input, want := range cases {
		if got := resolveModeName(input); got != want {
			t.Fatalf("resolveModeName(%q) = %q, want %q", input, got, want)
		}
	}
}

func TestQuoteIdentifierRejectsUnsafeNames(t *testing.T) {
	if _, err := debugutil.QuoteIdentifier("player_table"); err != nil {
		t.Fatalf("expected safe identifier to pass, got %v", err)
	}

	badNames := []string{"player-table", "player;drop", ""}
	for _, name := range badNames {
		if _, err := debugutil.QuoteIdentifier(name); err == nil {
			t.Fatalf("expected identifier %q to be rejected", name)
		}
	}
}

func TestRenderValueWithHintsDecodesKnownProto(t *testing.T) {
	msg := &database.PlayerDatabase{PlayerId: 42}
	raw, err := proto.Marshal(msg)
	if err != nil {
		t.Fatalf("marshal proto: %v", err)
	}

	decoded := renderValueWithHints("player_database_blob", raw, "")
	if decoded.Kind != "protobuf" {
		t.Fatalf("expected protobuf kind, got %q", decoded.Kind)
	}
	if decoded.ProtoType == "" {
		t.Fatalf("expected resolved proto type")
	}
	if decoded.ProtoJSON == nil {
		t.Fatalf("expected decoded proto json content")
	}
}
