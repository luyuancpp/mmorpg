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

// 邮件 tip 码的回归护栏(照 go/friend/internal/constants/constants_test.go)。
//
// mail 引用两条码轴:自己的 mail 段(17000),加上 common 段的参数非法 / 存储故障。
// "段内"断言按轴分开做(common 码当然不在 [17000,18000) 里,那是复用不是缺陷);
// 唯一性、被 serverbase 认得、定性、AST 护栏一律走 allTipCodes()。

// tipCodes 是本包 mail 段的 tip 码。往 Tip.xlsx 的 //mail_error 组加码时在这里补一行。
func tipCodes() map[string]uint32 {
	return map[string]uint32{
		"ErrClaimNotOpen":      ErrClaimNotOpen,
		"ErrNotFound":          ErrNotFound,
		"ErrExpired":           ErrExpired,
		"ErrRecipientBoxFull":  ErrRecipientBoxFull,
		"ErrAttachmentNotOpen": ErrAttachmentNotOpen,
		"ErrAttachmentInvalid": ErrAttachmentInvalid,
		"ErrRequestConflict":   ErrRequestConflict,
		"ErrSystemMailLimit":   ErrSystemMailLimit,
	}
}

// commonTipCodes 是本包复用的 common 段码(跨域语义,不在邮件段发号)。
func commonTipCodes() map[string]uint32 {
	return map[string]uint32{
		"ErrInvalidParameter": ErrInvalidParameter,
		"ErrStorage":          ErrStorage,
	}
}

// allTipCodes 是本包对外提供的全部 tip 码,给跨轴断言与 AST 护栏用。
func allTipCodes() map[string]uint32 {
	all := make(map[string]uint32, len(tipCodes())+len(commonTipCodes()))
	for name, code := range tipCodes() {
		all[name] = code
	}
	for name, code := range commonTipCodes() {
		all[name] = code
	}
	return all
}

// faultCodeNames:本包里 Tip.xlsx fault 列应为 1 的常量 —— 只有 ErrStorage(设计 §5.6:"mail 段零 fault 码")。
var faultCodeNames = map[string]struct{}{
	"ErrStorage": {},
}

// enumPrefixFor 返回某个 Err 常量**必须**引用的生成枚举前缀;ok=false = 还没纳入护栏。
func enumPrefixFor(name string) (prefix string, ok bool) {
	if _, isMail := tipCodes()[name]; isMail {
		return "MailError_", true
	}
	if _, isCommon := commonTipCodes()[name]; isCommon {
		return "CommonError_", true
	}
	return "", false
}

// segmentOf 从生成的段表里取某个域的段。段表源头是 data/tip/Tip.xlsx 的组头行。
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

// assertInSegment 断言码落在声明段 [Base, Base+Width) 内(只看声明,不看随新增码变化的 Lo/Hi)。
func assertInSegment(t *testing.T, name string, code uint32, domain string) {
	t.Helper()
	seg := segmentOf(t, domain)
	if code < seg.Base || code >= seg.Base+seg.Width {
		t.Errorf("%s = %d 落在 %s 段 [%d,%d) 之外(实际域=%s)",
			name, code, domain, seg.Base, seg.Base+seg.Width, serverbase.TipDomain(code))
	}
}

// TestMailSegmentIsTheDesignedOne 钉住段声明本身:base=17000 width=1000 是设计 §5.6 定的、Tip.xlsx 预留过的段。
// 段被人改成别的号,客户端按 id 查的文案就全错位,而别的用例(只看"码在段内")不会红。
func TestMailSegmentIsTheDesignedOne(t *testing.T) {
	seg := segmentOf(t, "mail")
	if seg.Base != 17000 || seg.Width != 1000 {
		t.Fatalf("mail 段应为 base=17000 width=1000,实际 base=%d width=%d", seg.Base, seg.Width)
	}
}

// TestTipCodesStayInMailSegment 钉死"邮件 tip 码只能落在自己的段里"。
func TestTipCodesStayInMailSegment(t *testing.T) {
	for name, code := range tipCodes() {
		assertInSegment(t, name, code, "mail")
	}
}

// TestCommonTipCodesAreCommonSegment 钉死"复用的两个码真的是 common 段的码":
// 有人把 ErrStorage 指到别的域的故障码上,按码聚合的告警就会漏掉 mail。
func TestCommonTipCodesAreCommonSegment(t *testing.T) {
	for name, code := range commonTipCodes() {
		assertInSegment(t, name, code, "common")
	}
}

// TestTipCodesAreRecognizedByServerbase:码进了配表就不该再被判成未知(否则每次拒绝都刷 rpc_inband_unknown_code)。
func TestTipCodesAreRecognizedByServerbase(t *testing.T) {
	for name, code := range allTipCodes() {
		if got := serverbase.TipVerdict(code); got == serverbase.VerdictUnknown {
			t.Errorf("%s = %d 仍被 serverbase 判成 VerdictUnknown —— 段表没跟上配表?", name, code)
		}
	}
}

// TestTipCodesDoNotCollideWithCommonSegment:邮件段的码不许和 common 段的低位码撞号(friend 那次事故的锚点)。
func TestTipCodesDoNotCollideWithCommonSegment(t *testing.T) {
	collided := []struct {
		name string
		code uint32
	}{
		{"kSuccess", uint32(table.CommonError_kSuccess)},
		{"kInvalidTableId", uint32(table.CommonError_kInvalidTableId)},
		{"kInvalidTableData", uint32(table.CommonError_kInvalidTableData)},
	}
	for name, code := range tipCodes() {
		for _, c := range collided {
			if code == c.code {
				t.Errorf("%s = %d 与 common 段的 %s 完全相同", name, code, c.name)
			}
		}
	}
}

// TestTipCodesAreUnique 防止段内自撞,也防止两条轴之间撞。
func TestTipCodesAreUnique(t *testing.T) {
	seen := map[uint32]string{}
	for name, code := range allTipCodes() {
		if prev, dup := seen[code]; dup {
			t.Errorf("tip 码 %d 被 %s 与 %s 同时使用", code, prev, name)
		}
		seen[code] = name
	}
}

// TestTipCodesVerdicts 钉住每个码的定性:邮件段 8 码 + 参数非法都是业务拒绝,**只有** ErrStorage 是故障。
// 有人给"收件人邮箱已满"之类的行在 Tip.xlsx 的 fault 列填了 1,这条先红,而不是等线上告警刷起来。
func TestTipCodesVerdicts(t *testing.T) {
	for name, code := range allTipCodes() {
		want := serverbase.VerdictBizReject
		if _, isFault := faultCodeNames[name]; isFault {
			want = serverbase.VerdictFault
		}
		if got := serverbase.TipVerdict(code); got != want {
			t.Errorf("%s = %d 应判 %v,实际 %v —— Tip.xlsx 的 fault 列被改了?", name, code, want, got)
		}
	}
}

// TestTipClassifier:本服务交出去的定性函数与全局判定一致,且轴外码仍判 VerdictUnknown。
func TestTipClassifier(t *testing.T) {
	c := TipClassifier()
	for name, code := range allTipCodes() {
		if got, want := c(code), serverbase.TipVerdict(code); got != want {
			t.Errorf("%s 的定性 %v 与全局判定 %v 不一致:TipClassifier 不该自己改定性", name, got, want)
		}
	}
	if got := c(uint32(table.CommonError_kServiceUnavailable)); got != serverbase.VerdictFault {
		t.Errorf("common 段的服务不可用应判 VerdictFault, 实际 %v", got)
	}
	if got := c(100000); got != serverbase.VerdictUnknown {
		t.Errorf("轴外码应判 VerdictUnknown, 实际 %v", got)
	}
}

// TestNoHandWrittenTipCodes:constants.go 里不许出现"Err* = 整数字面量",也不许跨轴写反(AGENTS §7 第 5 条)。
func TestNoHandWrittenTipCodes(t *testing.T) {
	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, "constants.go", nil, 0)
	if err != nil {
		t.Fatalf("解析 constants.go 失败: %v", err)
	}

	want := allTipCodes()
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
			prefix, registered := enumPrefixFor(name.Name)
			if !registered {
				continue
			}
			if i >= len(vs.Values) {
				t.Errorf("%s 必须显式写生成枚举引用,不能省略 RHS 继承上一常量", name.Name)
				continue
			}
			if !isGeneratedTipRef(vs.Values[i], prefix) {
				t.Errorf("%s 必须直接写成 uint32(table.%skX),不能手写数字、别名或引用其他码轴", name.Name, prefix)
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
			t.Errorf("constants.go 新增了 %s,必须同步纳入 tipCodes / commonTipCodes 的全量护栏", name)
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
		src    string
		prefix string
		want   bool
	}{
		{"uint32(table.MailError_kMailNotFound)", "MailError_", true},
		{"uint32(table.CommonError_kInvalidParameter)", "CommonError_", true},
		{"uint32(table.CommonError_kInvalidParameter)", "MailError_", false},
		{"uint32(table.FriendError_kFriendListFull)", "MailError_", false},
		{"uint32(17001)", "MailError_", false},
		{"17001", "MailError_", false},
		{"uint32(other.MailError_kMailNotFound)", "MailError_", false},
		{"uint64(table.MailError_kMailNotFound)", "MailError_", false},
	}
	for _, c := range cases {
		expr, err := parser.ParseExpr(c.src)
		if err != nil {
			t.Fatalf("解析 %q: %v", c.src, err)
		}
		if got := isGeneratedTipRef(expr, c.prefix); got != c.want {
			t.Errorf("isGeneratedTipRef(%s, %q) = %v, want %v", c.src, c.prefix, got, c.want)
		}
	}
}

// TestEnumPrefixForRejectsUnregistered:没登记的常量名必须拿不到前缀,否则"新增必须登记"形同虚设。
func TestEnumPrefixForRejectsUnregistered(t *testing.T) {
	if _, ok := enumPrefixFor("ErrSomethingNobodyRegistered"); ok {
		t.Error("未登记的常量名不该拿到枚举前缀")
	}
	if prefix, ok := enumPrefixFor("ErrNotFound"); !ok || prefix != "MailError_" {
		t.Errorf("邮件段常量应要求 MailError_,得到 (%q, %v)", prefix, ok)
	}
	if prefix, ok := enumPrefixFor("ErrStorage"); !ok || prefix != "CommonError_" {
		t.Errorf("common 段常量应要求 CommonError_,得到 (%q, %v)", prefix, ok)
	}
}
