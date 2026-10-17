package assetop

import (
	"context"
	"errors"
	"testing"

	assetpb "proto/common/asset"
	componentpb "proto/common/component"
)

func TestReceiptAckWaitsForIndependentSQLRemoval(t *testing.T) {
	op := testOp(1)
	op.Stream = assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT
	req := &assetpb.AssetOpRequest{PlayerId: op.PlayerID, Stream: op.Stream, StreamEpoch: op.StreamEpoch, Seq: op.Seq, CorrelationId: op.CorrelationID}
	for _, tc := range []struct {
		name    string
		reader  *fakeLedger
		receipt bool
		done    bool
	}{
		{"RPC成功但证据丢失", &fakeLedger{}, false, false},
		{"RPC成功但SQL尚未应用", sqlLedgerFixture(false, op), false, false},
		{"SQL仍保留原件", sqlLedgerFixture(true, op), true, false},
		{"SQL原操作已应用且原件已释放", sqlLedgerFixture(true, op), false, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if tc.receipt {
				tc.reader.ledger.Streams[0].Receipts = []*componentpb.AssetOpReceipt{{Seq: op.Seq, CorrelationId: op.CorrelationID}}
			}
			applier := &fakeApplier{fn: func(rpc RPC, got *assetpb.AssetOpRequest) (Result, error) {
				if rpc != RPCAckReceipt || got != req {
					t.Fatal("ACK不得改操作号或另占seq")
				}
				return resultApplied(true), nil
			}}
			done, err := AcknowledgeReceipt(context.Background(), applier, tc.reader, req)
			if err != nil || done != tc.done {
				t.Fatalf("done=%t err=%v", done, err)
			}
		})
	}
}

func TestReceiptAckFailureNeverCompletes(t *testing.T) {
	req := &assetpb.AssetOpRequest{PlayerId: 42, Stream: assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT, StreamEpoch: 1, Seq: 1, CorrelationId: 1}
	applier := &fakeApplier{fn: func(RPC, *assetpb.AssetOpRequest) (Result, error) { return Result{}, errors.New("答复未知") }}
	done, err := AcknowledgeReceipt(context.Background(), applier, &fakeLedger{}, req)
	if err == nil || done {
		t.Fatalf("不能把UNKNOWN当作已释放: %t %v", done, err)
	}
}

func TestReceiptCleanupPastWindowDoesNotLeavePermanentAckBacklog(t *testing.T) {
	op := testOp(1)
	op.Stream = assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT
	reader := sqlLedgerFixture(true, op)
	stream := reader.ledger.Streams[0]
	stream.Watermark = op.Seq
	stream.MaxSeq = op.Seq
	for i := range stream.SeenBits {
		stream.SeenBits[i] = 0
		stream.AppliedBits[i] = 0
	}
	req := &assetpb.AssetOpRequest{PlayerId: op.PlayerID, Stream: op.Stream, StreamEpoch: op.StreamEpoch, Seq: op.Seq, CorrelationId: op.CorrelationID}
	applier := &fakeApplier{fn: func(RPC, *assetpb.AssetOpRequest) (Result, error) { return resultApplied(true), nil }}
	done, err := AcknowledgeReceipt(context.Background(), applier, reader, req)
	if err != nil || !done {
		t.Fatalf("业务已存原件，SQL窗口已滑过且收据不存在，应结束清理: %t %v", done, err)
	}
}
