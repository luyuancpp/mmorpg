package store

import (
	"slices"
	"testing"
)

// TestTxLogRowsInLockOrder 钉住流水多行 INSERT IGNORE 的取锁顺序:按 tx_id 升序,且不改调用方的切片。
// 到达序拼批在 rebalance 重叠时会让两个实例在主键上反序互等(1213),见 InsertBatchIgnore 的注释;
// 真库并发对照(到达序 4/10 成环、升序 0/10)记在 docs/ops/incident-friend-lock-order-deadlock-2026-09-21.md §9.6。
func TestTxLogRowsInLockOrder(t *testing.T) {
	in := []*TransactionLogRow{{TxID: 30}, {TxID: 10}, {TxID: 20}, {TxID: 10, Extra: "second"}}
	before := slices.Clone(in)

	got := txLogRowsInLockOrder(in)

	var ids []uint64
	for _, r := range got {
		ids = append(ids, r.TxID)
	}
	if !slices.Equal(ids, []uint64{10, 10, 20, 30}) {
		t.Fatalf("必须按 tx_id 升序拼 VALUES,got %v", ids)
	}
	if got[0].Extra != "" || got[1].Extra != "second" {
		t.Fatalf("同 tx_id 的行必须保持原相对次序(稳定排序),got %q, %q", got[0].Extra, got[1].Extra)
	}
	if !slices.Equal(in, before) {
		t.Fatal("不许就地排序:调用方的批还要按原样提交 offset")
	}
}
