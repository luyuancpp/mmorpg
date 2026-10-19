package main

import (
	"strings"
	"testing"
)

// TestPlayerFieldIndexKeyMatchesDataService:拷 blob 前删的目标端字段索引必须与 data_service 的键逐字节相同
// (go/data_service/internal/logic/data_logic.go::fieldIndexKey),并落在同一个 {id} 前缀下。本 module 引不到
// data_service,两边各自钉同一个字面量:这里与 data_service 的 TestFieldIndexKeyLiteralPinnedForMergeZone 成对,改一边必须同批改另一边。
// 键名一漂移,DEL 删的是另一个键,目标端残留索引的戳与拷过去的 __version 巧合相等时,拷过去的字段从全量读里消失。
func TestPlayerFieldIndexKeyMatchesDataService(t *testing.T) {
	if got := playerFieldIndexKey(42); got != "player:{42}:__fields" {
		t.Fatalf("got %q", got)
	}
	if !strings.HasPrefix(playerFieldIndexKey(42), "player:{42}:") {
		t.Fatal("field index key left the player's hash-tag prefix")
	}
}
