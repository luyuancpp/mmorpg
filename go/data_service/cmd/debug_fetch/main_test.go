package main

import "testing"

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
	if decoded.Kind != "base64" {
		t.Fatalf("expected base64 kind, got %q", decoded.Kind)
	}
}
