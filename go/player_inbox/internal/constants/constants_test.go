package constants

import (
	"go/ast"
	"go/parser"
	"go/token"
	"strings"
	"testing"

	"shared/generated/tip"
	"shared/serverbase"
)

// tipCode 记录一个 Err 常量的码值与它应当落在的段。
type tipCode struct {
	code   uint32
	domain string // tip.Segment.Domain:"player_inbox" 或 "common"
	fault  bool   // Tip.xlsx fault 列应为 1
}

// tipCodes 是本包 Err 常量的全量清单,TestNoHandWrittenTipCodes 双向核对,漏补即红。
func tipCodes() map[string]tipCode {
	return map[string]tipCode{
		"ErrInvalidParameter":    {ErrInvalidParameter, "common", false},
		"ErrDisabled":            {ErrDisabled, "player_inbox", false},
		"ErrUnknownType":         {ErrUnknownType, "player_inbox", false},
		"ErrPlayerNotFound":      {ErrPlayerNotFound, "player_inbox", false},
		"ErrPayloadInvalid":      {ErrPayloadInvalid, "player_inbox", false},
		"ErrPayloadTooLarge":     {ErrPayloadTooLarge, "player_inbox", false},
		"ErrTextTooLong":         {ErrTextTooLong, "player_inbox", false},
		"ErrIssuedOutOfWindow":   {ErrIssuedOutOfWindow, "player_inbox", false},
		"ErrFull":                {ErrFull, "player_inbox", false},
		"ErrIdempotencyConflict": {ErrIdempotencyConflict, "player_inbox", false},
		"ErrStaleOwner":          {ErrStaleOwner, "player_inbox", false},
		"ErrEpochRequired":       {ErrEpochRequired, "player_inbox", false},
		"ErrEpochAhead":          {ErrEpochAhead, "player_inbox", false},
		"ErrItemNotFound":        {ErrItemNotFound, "player_inbox", false},
		"ErrItemStateConflict":   {ErrItemStateConflict, "player_inbox", false},
		"ErrStoreUnavailable":    {ErrStoreUnavailable, "player_inbox", true},
	}
}

// allowedTipPrefixes:本服务只许引用这两条码轴(见 constants.go 包注释)。
var allowedTipPrefixes = []string{"PlayerInboxError_", "CommonError_"}

func segmentOf(t *testing.T, domain string) tip.Segment {
	t.Helper()
	for _, s := range tip.Segments {
		if s.Domain == domain {
			return s
		}
	}
	t.Fatalf("生成的段表里没有 %s 段:Tip.xlsx 的 //%s_error 组是不是没导表?", domain, domain)
	return tip.Segment{}
}

// TestTipCodesStayInTheirSegments:收件箱码落在 player_inbox 段(base=29000)、复用的通用码落在 common 段。
func TestTipCodesStayInTheirSegments(t *testing.T) {
	for name, c := range tipCodes() {
		seg := segmentOf(t, c.domain)
		if c.code < seg.Base || c.code >= seg.Base+seg.Width {
			t.Errorf("%s = %d 落在 %s 段 [%d,%d) 之外(实际域=%s)",
				name, c.code, c.domain, seg.Base, seg.Base+seg.Width, serverbase.TipDomain(c.code))
		}
	}
	if seg := segmentOf(t, "player_inbox"); seg.Base != 29000 || seg.Width != 1000 {
		t.Errorf("player_inbox 段应为 base=29000 width=1000(设计 §5.3),实际 base=%d width=%d", seg.Base, seg.Width)
	}
}

// TestTipCodesAreRecognizedByServerbase:码进了配表就不该被判成未知(否则每次拒绝刷 rpc_inband_unknown_code)。
func TestTipCodesAreRecognizedByServerbase(t *testing.T) {
	for name, c := range tipCodes() {
		if got := serverbase.TipVerdict(c.code); got == serverbase.VerdictUnknown {
			t.Errorf("%s = %d 仍被 serverbase 判成 VerdictUnknown —— 段表没跟上配表?", name, c.code)
		}
	}
}

// TestTipCodesAreUnique:两个不同语义共用一个 id,运维 / scene 只会看到其中一个的含义。
func TestTipCodesAreUnique(t *testing.T) {
	seen := map[uint32]string{}
	for name, c := range tipCodes() {
		if prev, dup := seen[c.code]; dup {
			t.Errorf("tip 码 %d 被 %s 与 %s 同时使用", c.code, prev, name)
		}
		seen[c.code] = name
	}
}

// TestTipClassifier:只有 ErrStoreUnavailable 是故障,其余都是业务拒绝(设计 §5.3)。
// Tip.xlsx 的 fault 列被改时这里先红。
func TestTipClassifier(t *testing.T) {
	classify := TipClassifier()
	for name, c := range tipCodes() {
		want := serverbase.VerdictBizReject
		if c.fault {
			want = serverbase.VerdictFault
		}
		if got := classify(c.code); got != want {
			t.Errorf("%s = %d 应判 %v,实际 %v", name, c.code, want, got)
		}
	}
}

// TestNoHandWrittenTipCodes:Err 常量必须直接写成 uint32(table.PlayerInboxError_kX) 或 uint32(table.CommonError_kX)。
func TestNoHandWrittenTipCodes(t *testing.T) {
	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, "constants.go", nil, 0)
	if err != nil {
		t.Fatalf("解析 constants.go 失败: %v", err)
	}

	want := tipCodes()
	seen := make(map[string]struct{}, len(want))
	ast.Inspect(f, func(n ast.Node) bool {
		vs, ok := n.(*ast.ValueSpec)
		if !ok {
			return true
		}
		for i, name := range vs.Names {
			if !strings.HasPrefix(name.Name, "Err") {
				continue
			}
			seen[name.Name] = struct{}{}
			if i >= len(vs.Values) {
				t.Errorf("%s 必须显式写生成枚举引用,不能省略 RHS 继承上一常量", name.Name)
				continue
			}
			if !isGeneratedTipRef(vs.Values[i], allowedTipPrefixes) {
				t.Errorf("%s 必须直接写成 uint32(table.PlayerInboxError_kX) 或 uint32(table.CommonError_kX),"+
					"不能手写数字、别名或引用其他码轴", name.Name)
			}
		}
		return true
	})
	for name := range want {
		if _, ok := seen[name]; !ok {
			t.Errorf("tipCodes 声明了 %s,但 constants.go 没有对应 Err 常量", name)
		}
	}
	for name := range seen {
		if _, ok := want[name]; !ok {
			t.Errorf("constants.go 新增了 %s,必须同步纳入 tipCodes 的全量护栏", name)
		}
	}
}

func isGeneratedTipRef(expr ast.Expr, enumPrefixes []string) bool {
	call, ok := expr.(*ast.CallExpr)
	if !ok || len(call.Args) != 1 {
		return false
	}
	convert, ok := call.Fun.(*ast.Ident)
	if !ok || convert.Name != "uint32" {
		return false
	}
	selector, ok := call.Args[0].(*ast.SelectorExpr)
	if !ok {
		return false
	}
	pkg, ok := selector.X.(*ast.Ident)
	if !ok || pkg.Name != "table" {
		return false
	}
	for _, prefix := range enumPrefixes {
		if strings.HasPrefix(selector.Sel.Name, prefix) {
			return true
		}
	}
	return false
}

// TestIsGeneratedTipRefRejectsOtherShapes 保证护栏本身不是摆设:手写数字、别的码轴、别名都必须被拒。
func TestIsGeneratedTipRefRejectsOtherShapes(t *testing.T) {
	cases := map[string]bool{
		"uint32(table.PlayerInboxError_kPlayerInboxFull)": true,
		"uint32(table.CommonError_kInvalidParameter)":     true,
		"uint32(table.TradeError_kTradeListingNotFound)":  false,
		"uint32(29007)": false,
		"29007":         false,
		"uint32(other.PlayerInboxError_kPlayerInboxFull)": false,
		"uint64(table.PlayerInboxError_kPlayerInboxFull)": false,
	}
	for src, want := range cases {
		expr, err := parser.ParseExpr(src)
		if err != nil {
			t.Fatalf("解析 %q: %v", src, err)
		}
		if got := isGeneratedTipRef(expr, allowedTipPrefixes); got != want {
			t.Errorf("isGeneratedTipRef(%s) = %v, want %v", src, got, want)
		}
	}
}

// TestLimitsAreConsistent 钉住几条跨处约束:保留期之外的签发窗、Fetch 上限与环容量。
func TestLimitsAreConsistent(t *testing.T) {
	if FetchDefaultItems > FetchMaxItems || FetchMaxItems <= 0 {
		t.Errorf("FetchDefaultItems=%d 必须在 (0, FetchMaxItems=%d] 内", FetchDefaultItems, FetchMaxItems)
	}
	// 消费方「上一批全部确认前不拉下一批」:已处理未确认 ≤ 一批,32 条环必须装得下(设计 §4.4)。
	if AckMaxDecisions < FetchMaxItems {
		t.Errorf("AckMaxDecisions=%d 不得小于 FetchMaxItems=%d", AckMaxDecisions, FetchMaxItems)
	}
	if MaxPayloadBytesHardCap != 16*1024 {
		t.Errorf("MaxPayloadBytesHardCap=%d,设计 §3.4 硬上限是 16KiB", MaxPayloadBytesHardCap)
	}
}
