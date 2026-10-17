package constants

import (
	"go/ast"
	"go/parser"
	"go/token"
	"strings"
	"testing"

	"shared/generated/pb/table"
	"shared/generated/tip"
	"shared/serverbase"
)

// rank tip 码的回归护栏(照 go/mail / go/friend 的 constants_test.go)。v1 只引用 common 段(设计 §4.5,L-14),
// 所以只有一条码轴;将来开 21000 段时照 mail 的写法加一条 rankTipCodes 轴。

// commonTipCodes 是本包复用的 common 段码。
func commonTipCodes() map[string]uint32 {
	return map[string]uint32{
		"ErrInvalidParameter":   ErrInvalidParameter,
		"ErrServiceUnavailable": ErrServiceUnavailable,
	}
}

// faultCodeNames:Tip.xlsx fault 列应为 1 的常量 —— 只有 ErrServiceUnavailable。
var faultCodeNames = map[string]struct{}{
	"ErrServiceUnavailable": {},
}

func segmentOf(t *testing.T, domain string) tip.Segment {
	t.Helper()
	for _, s := range tip.Segments {
		if s.Domain == domain {
			return s
		}
	}
	t.Fatalf("生成的段表里没有 %s 段:Tip.xlsx 的 //%s_error 组是不是被删了 / 没导表?", domain, domain)
	return tip.Segment{}
}

// TestCommonTipCodesAreCommonSegment:复用的两个码真的是 common 段的码。有人把 ErrServiceUnavailable 指到别的域的
// 故障码上,按码聚合的告警就会漏掉 rank。
func TestCommonTipCodesAreCommonSegment(t *testing.T) {
	seg := segmentOf(t, "common")
	for name, code := range commonTipCodes() {
		if code < seg.Base || code >= seg.Base+seg.Width {
			t.Errorf("%s = %d 落在 common 段 [%d,%d) 之外(实际域=%s)", name, code, seg.Base, seg.Base+seg.Width, serverbase.TipDomain(code))
		}
	}
}

// TestRankSegmentNotOpenedInV1:v1 刻意不开 21000 段(L-14)。有人开了段却没在本包登记码轴,这条先红,提醒同步护栏。
func TestRankSegmentNotOpenedInV1(t *testing.T) {
	for _, s := range tip.Segments {
		if s.Domain == "rank" {
			t.Fatalf("Tip.xlsx 已开 rank 段 base=%d:按 constants.go 包注释加一条码轴与段内断言,再删掉本用例", s.Base)
		}
	}
}

// TestTipCodesVerdicts:参数非法是业务拒绝,服务不可用是故障。
func TestTipCodesVerdicts(t *testing.T) {
	for name, code := range commonTipCodes() {
		want := serverbase.VerdictBizReject
		if _, isFault := faultCodeNames[name]; isFault {
			want = serverbase.VerdictFault
		}
		if got := serverbase.TipVerdict(code); got != want {
			t.Errorf("%s = %d 应判 %v,实际 %v —— Tip.xlsx 的 fault 列被改了?", name, code, want, got)
		}
	}
}

// TestTipCodesAreUnique:两个码不许撞(把 ErrServiceUnavailable 误写成参数非法,告警就没了)。
func TestTipCodesAreUnique(t *testing.T) {
	if ErrInvalidParameter == ErrServiceUnavailable {
		t.Fatalf("ErrInvalidParameter 与 ErrServiceUnavailable 不能是同一个码(%d)", ErrInvalidParameter)
	}
}

// TestTipClassifier:本服务交出去的定性函数与全局判定一致,轴外码仍判 VerdictUnknown。
func TestTipClassifier(t *testing.T) {
	c := TipClassifier()
	for name, code := range commonTipCodes() {
		if got, want := c(code), serverbase.TipVerdict(code); got != want {
			t.Errorf("%s 的定性 %v 与全局判定 %v 不一致", name, got, want)
		}
	}
	if got := c(100000); got != serverbase.VerdictUnknown {
		t.Errorf("轴外码应判 VerdictUnknown, 实际 %v", got)
	}
}

// TestNoHandWrittenTipCodes:constants.go 里不许出现"Err* = 整数字面量",也不许引用 CommonError_ 之外的轴(AGENTS §7 第 5 条)。
func TestNoHandWrittenTipCodes(t *testing.T) {
	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, "constants.go", nil, 0)
	if err != nil {
		t.Fatalf("解析 constants.go 失败: %v", err)
	}
	want := commonTipCodes()
	seen := map[string]struct{}{}
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
			if !isGeneratedTipRef(vs.Values[i], "CommonError_") {
				t.Errorf("%s 必须直接写成 uint32(table.CommonError_kX),不能手写数字或引用其他码轴", name.Name)
			}
		}
		return true
	})
	for name := range want {
		if _, ok := seen[name]; !ok {
			t.Errorf("护栏清单声明了 %s,但 constants.go 没有对应 Err 常量", name)
		}
	}
	for name := range seen {
		if _, ok := want[name]; !ok {
			t.Errorf("constants.go 新增了 %s,必须同步纳入 commonTipCodes 护栏", name)
		}
	}
}

func isGeneratedTipRef(expr ast.Expr, enumPrefix string) bool {
	call, ok := expr.(*ast.CallExpr)
	if !ok || len(call.Args) != 1 {
		return false
	}
	convert, ok := call.Fun.(*ast.Ident)
	if !ok || convert.Name != "uint32" {
		return false
	}
	selector, ok := call.Args[0].(*ast.SelectorExpr)
	if !ok || !strings.HasPrefix(selector.Sel.Name, enumPrefix) {
		return false
	}
	pkg, ok := selector.X.(*ast.Ident)
	return ok && pkg.Name == "table"
}

// TestIsGeneratedTipRefRejectsOtherShapes 保证护栏本身不是摆设。
func TestIsGeneratedTipRefRejectsOtherShapes(t *testing.T) {
	cases := []struct {
		src  string
		want bool
	}{
		{"uint32(table.CommonError_kInvalidParameter)", true},
		{"uint32(table.MailError_kMailNotFound)", false},
		{"uint32(1005)", false},
		{"1005", false},
		{"uint32(other.CommonError_kInvalidParameter)", false},
		{"uint64(table.CommonError_kInvalidParameter)", false},
	}
	for _, c := range cases {
		expr, err := parser.ParseExpr(c.src)
		if err != nil {
			t.Fatalf("解析 %q: %v", c.src, err)
		}
		if got := isGeneratedTipRef(expr, "CommonError_"); got != c.want {
			t.Errorf("isGeneratedTipRef(%s) = %v, want %v", c.src, got, c.want)
		}
	}
	// 本包当前码表与生成表一致(防有人把 const 改成别的 common 码)。
	if ErrInvalidParameter != uint32(table.CommonError_kInvalidParameter) || ErrServiceUnavailable != uint32(table.CommonError_kServiceUnavailable) {
		t.Fatal("常量与生成枚举不一致")
	}
}
