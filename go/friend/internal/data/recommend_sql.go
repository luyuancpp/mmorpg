package data

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"friend/internal/sqldialect"
)

// recommend_sql.go —— 好友推荐两条查询的"按方言渲染"。
//
// recommend_repo.go 里的 recommendByMutualBaseSQL / recommendAnchorBaseSQL 是**模板**:业务 SQL 只有一份,
// 优化器提示的位置留成占位符(`{{hint.outer}}` 等),由本文件按方言填进去。为什么不能两边共用一份带提示的文本:
//
//   - 两条查询的扫描上界在 MySQL 与 TiDB 上都靠提示钉住,而两边的提示名互不相认:
//     MySQL 用 SEMIJOIN(FIRSTMATCH) 把五条排除钉成"逐候选主键点查";TiDB 不认它(警告 8061 后忽略),要用
//     INL_JOIN 把同样五条排除、以及 RecommendByMutual 内层的 f2 钉成索引连接,MySQL 又不认 INL_JOIN(1064)。
//   - 把两套提示写进同一份文本也跑得通(对方不认的那套只是变成警告),但那样每一次调用在两边都带着警告 ——
//     语句级警告会进各自的语句摘要 / 监控,真有问题时被淹没;而且"这条提示在这一边到底生效没有"无从在文本上看出来。
//
// 所以每种方言一份文本,启动期探测方言后选定(sqldialect.Detect → NewFriendRepo),请求路径上不再判断。
//
// TiDB 不加提示时会怎样(2026-10-09 / 10-10 / 10-13 在 v8.5.2 上实测,数据与逐项数字见
// docs/handoff/friend-handoff-20260920.md §9.5 第 12 条):统计新鲜时计划有界,但那是代价估算的结果;统计低估表规模时
// (建表后锁统计再灌数、或统计停在表还很小的时候)b_in / r_in 变成对 friend_block / friend_request 的整表扫描,
// RecommendByMutual 的 f2 变成对 friend 的整表扫描。加上本文件的提示后,13 种库 / 统计状态下(10-09 那 12 种,
// 含一个聚簇主键的库,再加一个按 Go 用例灌数顺序造的库)两条查询都不再有整表扫描,五条排除与 f2 全是按连接键定位的
// 主键范围读,结果与 MySQL 文本在同一个库上逐行相同,文本协议与预处理协议都没有提示类警告
// (TiDB 文本里也不再带 SEMIJOIN,所以 8061 一并消失)。
//
// 语句超时(两种方言都有):最外层另带 MAX_EXECUTION_TIME(毫秒),见 newRecommendStatements。

// 提示位占位符。花括号形式是刻意的:漏替换时它是一处 SQL 语法错,而不是一条被悄悄忽略的注释。
const (
	recommendHintSlotOuter   = "{{hint.outer}}"   // 最外层 SELECT 之后
	recommendHintSlotDerived = "{{hint.derived}}" // RecommendByMutual 派生表的 SELECT 之后
)

// recommendExclusionCount 是排除子查询的条数;recommendExclusionAliases 是它们的别名,顺序与模板里出现的顺序一致。
// 每条排除的占位符是 `{{hint.<别名>}}`,TiDB 的查询块名是 `qb_<别名>`。别名改了要同时改模板与这张表 ——
// 漏改时渲染会 panic(占位符没被替换),不会带着半截提示上线。
const recommendExclusionCount = 5

var recommendExclusionAliases = [recommendExclusionCount]string{"f", "b_out", "b_in", "r_out", "r_in"}

// recommendFOFQueryBlock 是 RecommendByMutual 派生表(好友的好友)在 TiDB 上的查询块名。
const recommendFOFQueryBlock = "qb_fof"

func recommendExclusionHintSlot(alias string) string { return "{{hint." + alias + "}}" }

// recommendHints 是一种方言下各个提示位要放的提示(不含 `/*+ */` 定界符;空串 = 这个位置不放)。
type recommendHints struct {
	anchorOuter   string
	mutualOuter   string
	mutualDerived string
	exclusion     [recommendExclusionCount]string
}

// recommendHintsFor 返回某种方言的提示表。每一项为什么是这样、去掉会怎样:
//
// MySQL:五条排除各带 SEMIJOIN(FIRSTMATCH)(recommend_repo.go 两个方法注释的第 5 条);外层与派生表不放连接提示。
//
// TiDB(2026-10-10 在 v8.5.2 上逐项试出来的写法):
//   - 五条排除各起一个查询块名 QB_NAME(qb_<别名>),外层用 INL_JOIN(<别名>@qb_<别名>, ...) 把它们钉成索引连接。
//     提示必须写在外层:写进子查询自己的提示注释里,TiDB 报 1815(找不到表名)、不生效。
//   - RecommendByMutual 的派生表起名 QB_NAME(qb_fof),外层用 INL_JOIN(@qb_fof f2) 钉住 f2。
//     试过的另外两种写法都不行:在派生表里直接写 INL_JOIN(f2) —— 生效,但预处理协议下每次 PREPARE 都报 1815(一次五条);
//     外层写成 INL_JOIN(f2@qb_fof) —— 报 1815 且不生效(统计停在小表时 f2 仍然整表扫描)。
//   - 不带 SEMIJOIN:TiDB 不认,带着只会每条语句多五条 8061。
//   - FORCE INDEX (PRIMARY) 与 STRAIGHT_JOIN 在模板正文里,两种方言共用,TiDB 都认。
func recommendHintsFor(dialect sqldialect.Dialect) recommendHints {
	var h recommendHints
	switch dialect {
	case sqldialect.MySQL:
		for i := range h.exclusion {
			h.exclusion[i] = "SEMIJOIN(FIRSTMATCH)"
		}
	case sqldialect.TiDB:
		refs := make([]string, 0, recommendExclusionCount)
		for i, alias := range recommendExclusionAliases {
			h.exclusion[i] = "QB_NAME(qb_" + alias + ")"
			refs = append(refs, alias+"@qb_"+alias)
		}
		h.anchorOuter = "INL_JOIN(" + strings.Join(refs, ", ") + ")"
		h.mutualOuter = h.anchorOuter + " INL_JOIN(@" + recommendFOFQueryBlock + " f2)"
		h.mutualDerived = "QB_NAME(" + recommendFOFQueryBlock + ")"
	default:
		// 方言是封闭枚举,走到这里只可能是加了新方言却没给提示表:启动期就炸,不带着没有提示的 SQL 上线。
		panic(fmt.Sprintf("friend data: no optimizer hints defined for SQL dialect %s in recommendation queries", dialect))
	}
	return h
}

// recommendHintComment 把若干条提示拼成一个 `/*+ a b */ ` 注释(带尾随空格);全是空串时返回空串,
// 这样"这个位置不放提示"渲染出来就是原样的 `SELECT 列...`,不留一个空注释。
func recommendHintComment(hints ...string) string {
	kept := make([]string, 0, len(hints))
	for _, h := range hints {
		if h != "" {
			kept = append(kept, h)
		}
	}
	if len(kept) == 0 {
		return ""
	}
	return "/*+ " + strings.Join(kept, " ") + " */ "
}

// recommendStatements 是按方言渲染好的两条查询主体(调用方 exclude 与 ORDER BY / LIMIT 由 byMutual / anchor 拼在后面)。
// 值类型、构造后只读:NewFriendRepo 渲染一次,之后所有请求共用。
type recommendStatements struct {
	mutualBase string
	anchorBase string
}

// newRecommendStatements 渲染两条查询。
//
// statementTimeout 是**服务端**的语句执行上限,渲染成最外层的 MAX_EXECUTION_TIME(毫秒)(MySQL 与 TiDB 都认这条提示)。
// 它补的是 context 截止时间补不到的一段:请求预算到期时 go-sql-driver 只是关掉连接,服务器要等到下一次往连接上写数据
// 才发现对端没了 —— 一条退化成大范围扫描、迟迟不出结果的 SELECT 会在没人等它的情况下继续跑完。带上服务端上限后,
// 最坏情况是它在 statementTimeout 之后被服务器自己掐掉,调用方照常按存储故障处理(logic 不区分错误码)。
// 掐掉时的错误码:MySQL 是 3024;TiDB **两种都会出** —— 1105 "context canceled" 或 3024,同一条语句、同一个上限
// 连跑几次就会混着出现(2026-10-13 在 v8.5.2 上用真实的兜底语句实测 35 次超时:1105 共 28 次、3024 共 7 次)。
// 以后要按错误码认"语句超时"(日志检索、告警、错误分类),两种都得认。
// 装配层传的是整请求预算(config.RequestBudget):正常路径上 context 总是先到期,这条上限不改变任何可见行为。
// ≤ 0(或不足 1 毫秒)表示不带上限,只给不关心它的单测用。
func newRecommendStatements(dialect sqldialect.Dialect, statementTimeout time.Duration) recommendStatements {
	hints := recommendHintsFor(dialect)
	timeoutHint := ""
	if ms := statementTimeout.Milliseconds(); ms > 0 {
		timeoutHint = "MAX_EXECUTION_TIME(" + strconv.FormatInt(ms, 10) + ")"
	}
	return recommendStatements{
		mutualBase: renderRecommendSQL(recommendByMutualBaseSQL,
			recommendHintComment(timeoutHint, hints.mutualOuter), recommendHintComment(hints.mutualDerived), hints.exclusion),
		anchorBase: renderRecommendSQL(recommendAnchorBaseSQL,
			recommendHintComment(timeoutHint, hints.anchorOuter), "", hints.exclusion),
	}
}

// renderRecommendSQL 把模板里的提示位换成已经拼好的提示注释。
// 渲染完还剩占位符(模板里出现了提示表不认识的位置)就 panic:那是改模板时漏了这边,必须在启动 / 单测时暴露。
func renderRecommendSQL(template, outerComment, derivedComment string, exclusion [recommendExclusionCount]string) string {
	pairs := make([]string, 0, 2*(2+recommendExclusionCount))
	pairs = append(pairs, recommendHintSlotOuter, outerComment, recommendHintSlotDerived, derivedComment)
	for i, alias := range recommendExclusionAliases {
		pairs = append(pairs, recommendExclusionHintSlot(alias), recommendHintComment(exclusion[i]))
	}
	rendered := strings.NewReplacer(pairs...).Replace(template)
	if strings.Contains(rendered, "{{") {
		panic("friend data: recommendation SQL template has an unreplaced hint slot: " + rendered)
	}
	return rendered
}
