package cpp

import (
	"strings"
	"testing"
)

// event_id.txt 与 message_id.txt 共用的行解析与重号校验(id_file.go)。用例自 event_id_test.go 挪来,内容不变。

// mustLoadIdFile 把 `N=名` / `N=reserved:原名` 逐行登记进 idFileState;任何一行解析失败或重号都让用例失败。
func mustLoadIdFile(t *testing.T, content string) *idFileState {
	t.Helper()
	state := newIdFileState()
	if content == "" {
		return state
	}
	for _, line := range strings.Split(strings.TrimRight(content, "\n"), "\n") {
		entry, ok, err := parseIdFileLine(line)
		if err != nil {
			t.Fatalf("parse %q: %v", line, err)
		}
		if !ok {
			t.Fatalf("unexpected invalid line %q", line)
		}
		if err := state.add(entry); err != nil {
			t.Fatalf("add %q: %v", line, err)
		}
	}
	return state
}

func TestParseIdFileLine(t *testing.T) {
	cases := []struct {
		line   string
		want   idFileEntry
		wantOK bool
	}{
		{line: "7=CombatStateAddedEvent", want: idFileEntry{Id: 7, Name: "CombatStateAddedEvent"}, wantOK: true},
		{line: " 43 = reserved:ContractsKafkaBindBattleEvent\r", want: idFileEntry{Id: 43, Name: "ContractsKafkaBindBattleEvent", Reserved: true}, wantOK: true},
		{line: "44=reserved: ContractsKafkaUnbindBattleEvent", want: idFileEntry{Id: 44, Name: "ContractsKafkaUnbindBattleEvent", Reserved: true}, wantOK: true},
		{line: "", wantOK: false},
		{line: "no equals sign", wantOK: false},
		{line: "1=a=b", wantOK: false},
	}
	for _, tc := range cases {
		got, ok, err := parseIdFileLine(tc.line)
		if err != nil {
			t.Fatalf("parseIdFileLine(%q) unexpected error: %v", tc.line, err)
		}
		if ok != tc.wantOK {
			t.Fatalf("parseIdFileLine(%q) ok = %v, want %v", tc.line, ok, tc.wantOK)
		}
		if ok && got != tc.want {
			t.Fatalf("parseIdFileLine(%q) = %+v, want %+v", tc.line, got, tc.want)
		}
	}

	if _, _, err := parseIdFileLine("x=SomeEvent"); err == nil {
		t.Fatal("non-numeric id must be an error, got nil")
	}
}

func TestIdFileStateRejectsDuplicates(t *testing.T) {
	cases := []struct {
		name    string
		content string
		bad     string
	}{
		{name: "same id twice", content: "3=A", bad: "3=B"},
		{name: "live id also reserved", content: "3=A", bad: "3=reserved:B"},
		{name: "reserved id also live", content: "3=reserved:A", bad: "3=B"},
		{name: "reserved id twice", content: "3=reserved:A", bad: "3=reserved:B"},
		{name: "same live name twice", content: "3=A", bad: "4=A"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			state := mustLoadIdFile(t, tc.content)
			entry, ok, err := parseIdFileLine(tc.bad)
			if err != nil || !ok {
				t.Fatalf("test line %q must parse: ok=%v err=%v", tc.bad, ok, err)
			}
			if err := state.add(entry); err == nil {
				t.Fatalf("add(%q) after %q must fail", tc.bad, tc.content)
			}
		})
	}

	// 同名的活行与墓碑可以共存:删掉后又加回同名条目,拿的是新号,墓碑照旧占着老号。
	state := mustLoadIdFile(t, "1=reserved:B\n2=B\n")
	if state.Active["B"] != 2 || state.Reserved[1] != "B" || state.MaxId != 2 {
		t.Fatalf("unexpected state: %+v", state)
	}
}
