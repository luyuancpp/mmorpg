package data

// commerce_leg_outcome_test.go —— 合同腿结局分类的回归(2026-10-20,第一轮审计 §2.5 go-asset-services#1),不连库。
//
// 以前 CommerceRepo.LegResult 只认 durable=1 的终局;ResolveManually 与 Abandon 刻意不置 durable,
// 于是人工终结、自动放弃的托管扣款腿永远读不到,合同停在 pay / escrow,另一方托管的资产被扣住。
// 这里钉住"终局只看 status、不看 durable 与来源"。端到端(放弃 → 合同补偿只一次)在
// internal/commerce/tick_isolation_db_test.go 的真库用例里。

import (
	"testing"

	tradepb "proto/trade"

	"github.com/stretchr/testify/require"
)

func TestLegOutcomeFinalityDoesNotDependOnSource(t *testing.T) {
	cases := []struct {
		name    string
		out     LegOutcome
		pending bool
		final   bool
		source  string
	}{
		{"待办", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING}, true, false, "scene"},
		{"scene 落盘成功", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED}, false, true, "scene"},
		{"scene 判拒", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_REJECTED}, false, true, "scene"},
		{"人工判定成功", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED, ResolvedBy: "ops"}, false, true, "manual"},
		{"人工判定中止", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABORTED, ResolvedBy: "ops"}, false, true, "manual"},
		{"部分发放", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED_PARTIAL}, false, true, "scene"},
		{"自动放弃", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED}, false, true, "abandoned"},
		{"零值坏行", LegOutcome{Status: tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED}, false, false, "scene"},
		{"未知状态", LegOutcome{Status: tradepb.TradeAssetOpStatus(99)}, false, false, "scene"},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			require.Equal(t, c.pending, c.out.Pending())
			require.Equal(t, c.final, c.out.Final())
			require.Equal(t, c.source, c.out.Source())
		})
	}
}

// 枚举新增状态时必须回来决定它算不算合同腿的终局:除 UNSPECIFIED / PENDING 之外的每个值都必须 Final,
// 否则合同推进会把它当坏数据隔离(或者更糟,像 2026-10-20 之前那样永远等下去)。
func TestEveryTerminalAssetOpStatusIsFinalForContracts(t *testing.T) {
	for value := range tradepb.TradeAssetOpStatus_name {
		st := tradepb.TradeAssetOpStatus(value)
		out := LegOutcome{Status: st}
		switch st {
		case tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED:
			require.False(t, out.Final(), st.String())
			require.False(t, out.Pending(), st.String())
		case tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING:
			require.False(t, out.Final(), st.String())
			require.True(t, out.Pending(), st.String())
		default:
			require.True(t, out.Final(), "%s 是 outbox 终态,合同推进必须认它", st.String())
			require.False(t, out.Pending(), st.String())
		}
	}
}
