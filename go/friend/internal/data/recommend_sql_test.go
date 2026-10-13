package data

// recommend_sql_test.go —— 好友推荐 SQL "按方言渲染"的纯单测(不连库)。
//
// 这里钉三件事:
//  1. **MySQL 那份文本没有被方言化改动**:模板按 MySQL 渲染(不带语句超时)出来,必须与下面的 golden 逐字节相同。
//     这份 golden 就是 2026-10-10 之前 recommendByMutualBaseSQL / recommendAnchorBaseSQL 两个常量的原文 ——
//     recommend_repo.go 里所有的上界推导、实测读数、MySQL 真库用例的计划断言说的都是它。
//  2. **TiDB 那份文本就是在 TiDB 上实测过的那一份**(2026-10-10 / 10-13,v8.5.2:13 种库 / 统计状态下无整表扫描、
//     文本与预处理协议都没有提示类警告、结果与 MySQL 文本在同一个库上逐行相同)。提示改一个字符都要回 TiDB 重测,
//     所以同样逐字节钉住。
//  3. 两种方言**只在提示上不同**:去掉 `/*+ ... */` 之后是同一条 SQL,占位符个数与顺序相同 ——
//     byMutual / anchor 只有一套参数顺序,这一条不成立就会在某一边把参数绑错位。
//
// 改 SQL 时的做法:先改 recommend_repo.go 的模板与 recommend_sql.go 的提示表,在两种库上重测(MySQL 的真库用例 +
// recommend_repo_tidb_test.go),再把这里的 golden 换成新文本。golden 不是"跑一遍把输出贴回来"的快照:
// 贴之前要能说出每一处差异为什么是对的。

import (
	"regexp"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"friend/internal/sqldialect"
)

// 下面四份 golden 的说明见文件头。它们是原始字符串,行尾空白与换行都是文本的一部分,不要让编辑器动它。

const goldenRecommendByMutualMySQL = `SELECT c.candidate_id, c.mutual
FROM (SELECT f2.friend_player_id AS candidate_id, COUNT(*) AS mutual, RAND() AS shuffle
      FROM friend f1 FORCE INDEX (PRIMARY)
      STRAIGHT_JOIN friend f2 FORCE INDEX (PRIMARY) ON f2.player_id = f1.friend_player_id
      WHERE f1.player_id = ?
        AND f2.friend_player_id <> ?
      GROUP BY f2.friend_player_id
      ORDER BY mutual DESC, shuffle
      LIMIT ?) AS c
WHERE NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend f FORCE INDEX (PRIMARY)
        WHERE f.player_id = ? AND f.friend_player_id = c.candidate_id)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_block b_out FORCE INDEX (PRIMARY)
        WHERE b_out.player_id = ? AND b_out.blocked_player_id = c.candidate_id)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_block b_in FORCE INDEX (PRIMARY)
        WHERE b_in.player_id = c.candidate_id AND b_in.blocked_player_id = ?)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_request r_out FORCE INDEX (PRIMARY)
        WHERE r_out.from_player_id = ? AND r_out.to_player_id = c.candidate_id AND r_out.status = 1)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_request r_in FORCE INDEX (PRIMARY)
        WHERE r_in.from_player_id = c.candidate_id AND r_in.to_player_id = ? AND r_in.status = 1)`

const goldenRecommendAnchorMySQL = `SELECT c.player_id,
       (SELECT COUNT(*) FROM friend m1 FORCE INDEX (PRIMARY)
        STRAIGHT_JOIN friend m2 FORCE INDEX (PRIMARY)
                ON m2.player_id = c.player_id AND m2.friend_player_id = m1.friend_player_id
        WHERE m1.player_id = ?) AS mutual
FROM (SELECT DISTINCT player_id FROM friend FORCE INDEX (PRIMARY)
      WHERE player_id >= ?
      ORDER BY player_id
      LIMIT ?) AS c
WHERE c.player_id <> ?
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend f FORCE INDEX (PRIMARY)
        WHERE f.player_id = ? AND f.friend_player_id = c.player_id)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_block b_out FORCE INDEX (PRIMARY)
        WHERE b_out.player_id = ? AND b_out.blocked_player_id = c.player_id)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_block b_in FORCE INDEX (PRIMARY)
        WHERE b_in.player_id = c.player_id AND b_in.blocked_player_id = ?)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_request r_out FORCE INDEX (PRIMARY)
        WHERE r_out.from_player_id = ? AND r_out.to_player_id = c.player_id AND r_out.status = 1)
  AND NOT EXISTS (SELECT /*+ SEMIJOIN(FIRSTMATCH) */ 1 FROM friend_request r_in FORCE INDEX (PRIMARY)
        WHERE r_in.from_player_id = c.player_id AND r_in.to_player_id = ? AND r_in.status = 1)`

const goldenRecommendByMutualTiDB = `SELECT /*+ INL_JOIN(f@qb_f, b_out@qb_b_out, b_in@qb_b_in, r_out@qb_r_out, r_in@qb_r_in) INL_JOIN(@qb_fof f2) */ c.candidate_id, c.mutual
FROM (SELECT /*+ QB_NAME(qb_fof) */ f2.friend_player_id AS candidate_id, COUNT(*) AS mutual, RAND() AS shuffle
      FROM friend f1 FORCE INDEX (PRIMARY)
      STRAIGHT_JOIN friend f2 FORCE INDEX (PRIMARY) ON f2.player_id = f1.friend_player_id
      WHERE f1.player_id = ?
        AND f2.friend_player_id <> ?
      GROUP BY f2.friend_player_id
      ORDER BY mutual DESC, shuffle
      LIMIT ?) AS c
WHERE NOT EXISTS (SELECT /*+ QB_NAME(qb_f) */ 1 FROM friend f FORCE INDEX (PRIMARY)
        WHERE f.player_id = ? AND f.friend_player_id = c.candidate_id)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_b_out) */ 1 FROM friend_block b_out FORCE INDEX (PRIMARY)
        WHERE b_out.player_id = ? AND b_out.blocked_player_id = c.candidate_id)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_b_in) */ 1 FROM friend_block b_in FORCE INDEX (PRIMARY)
        WHERE b_in.player_id = c.candidate_id AND b_in.blocked_player_id = ?)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_r_out) */ 1 FROM friend_request r_out FORCE INDEX (PRIMARY)
        WHERE r_out.from_player_id = ? AND r_out.to_player_id = c.candidate_id AND r_out.status = 1)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_r_in) */ 1 FROM friend_request r_in FORCE INDEX (PRIMARY)
        WHERE r_in.from_player_id = c.candidate_id AND r_in.to_player_id = ? AND r_in.status = 1)`

const goldenRecommendAnchorTiDB = `SELECT /*+ INL_JOIN(f@qb_f, b_out@qb_b_out, b_in@qb_b_in, r_out@qb_r_out, r_in@qb_r_in) */ c.player_id,
       (SELECT COUNT(*) FROM friend m1 FORCE INDEX (PRIMARY)
        STRAIGHT_JOIN friend m2 FORCE INDEX (PRIMARY)
                ON m2.player_id = c.player_id AND m2.friend_player_id = m1.friend_player_id
        WHERE m1.player_id = ?) AS mutual
FROM (SELECT DISTINCT player_id FROM friend FORCE INDEX (PRIMARY)
      WHERE player_id >= ?
      ORDER BY player_id
      LIMIT ?) AS c
WHERE c.player_id <> ?
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_f) */ 1 FROM friend f FORCE INDEX (PRIMARY)
        WHERE f.player_id = ? AND f.friend_player_id = c.player_id)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_b_out) */ 1 FROM friend_block b_out FORCE INDEX (PRIMARY)
        WHERE b_out.player_id = ? AND b_out.blocked_player_id = c.player_id)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_b_in) */ 1 FROM friend_block b_in FORCE INDEX (PRIMARY)
        WHERE b_in.player_id = c.player_id AND b_in.blocked_player_id = ?)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_r_out) */ 1 FROM friend_request r_out FORCE INDEX (PRIMARY)
        WHERE r_out.from_player_id = ? AND r_out.to_player_id = c.player_id AND r_out.status = 1)
  AND NOT EXISTS (SELECT /*+ QB_NAME(qb_r_in) */ 1 FROM friend_request r_in FORCE INDEX (PRIMARY)
        WHERE r_in.from_player_id = c.player_id AND r_in.to_player_id = ? AND r_in.status = 1)`

func TestRecommendStatements_RenderedTextIsPinnedPerDialect(t *testing.T) {
	mysql := newRecommendStatements(sqldialect.MySQL, 0)
	assert.Equal(t, goldenRecommendByMutualMySQL, mysql.mutualBase, "RecommendByMutual 的 MySQL 文本变了:上界注释与 MySQL 真库用例说的都是 golden 这一份")
	assert.Equal(t, goldenRecommendAnchorMySQL, mysql.anchorBase, "recommendAnchor 的 MySQL 文本变了")

	tidb := newRecommendStatements(sqldialect.TiDB, 0)
	assert.Equal(t, goldenRecommendByMutualTiDB, tidb.mutualBase, "RecommendByMutual 的 TiDB 文本变了:要回 TiDB 重测后再改 golden")
	assert.Equal(t, goldenRecommendAnchorTiDB, tidb.anchorBase, "recommendAnchor 的 TiDB 文本变了:要回 TiDB 重测后再改 golden")
}

// recommendHintCommentRe 匹配一个优化器提示注释连同它后面的那个空格(渲染时两者总是一起出现)。
var recommendHintCommentRe = regexp.MustCompile(`/\*\+.*?\*/ `)

func TestRecommendStatements_DialectsDifferOnlyInHints(t *testing.T) {
	mysql := newRecommendStatements(sqldialect.MySQL, 3500*time.Millisecond)
	tidb := newRecommendStatements(sqldialect.TiDB, 3500*time.Millisecond)
	pairs := []struct {
		name        string
		mysql, tidb string
	}{
		{"RecommendByMutual", mysql.mutualBase, tidb.mutualBase},
		{"recommendAnchor", mysql.anchorBase, tidb.anchorBase},
	}
	for _, p := range pairs {
		assert.Equal(t, recommendHintCommentRe.ReplaceAllString(p.mysql, ""), recommendHintCommentRe.ReplaceAllString(p.tidb, ""),
			"%s:去掉提示注释后两种方言必须是同一条 SQL", p.name)
		assert.Equal(t, strings.Count(p.mysql, "?"), strings.Count(p.tidb, "?"), "%s:两种方言的占位符个数必须相同(参数只有一套顺序)", p.name)
		for _, sql := range []string{p.mysql, p.tidb} {
			assert.NotContains(t, sql, "{{", "%s:渲染后不许剩占位符", p.name)
			for _, comment := range recommendHintCommentRe.FindAllString(sql, -1) {
				assert.NotContains(t, comment, "?", "%s:提示注释里不许有参数占位符(会把参数顺序挤错位): %s", p.name, comment)
			}
		}
		assert.NotContains(t, p.tidb, "SEMIJOIN", "%s:TiDB 不认 SEMIJOIN(每条语句五条 8061 警告),TiDB 文本里不该有", p.name)
		assert.NotContains(t, p.mysql, "INL_JOIN", "%s:MySQL 不认 INL_JOIN(1064 警告),MySQL 文本里不该有", p.name)
		assert.NotContains(t, p.mysql, "QB_NAME", "%s:MySQL 文本不需要查询块名", p.name)
	}
}

func TestRecommendStatements_StatementTimeoutHint(t *testing.T) {
	for _, d := range sqldialect.All() {
		without := newRecommendStatements(d, 0)
		with := newRecommendStatements(d, 3500*time.Millisecond)
		for _, p := range []struct{ name, without, with string }{
			{"RecommendByMutual", without.mutualBase, with.mutualBase},
			{"recommendAnchor", without.anchorBase, with.anchorBase},
		} {
			require.True(t, strings.HasPrefix(p.with, "SELECT /*+ MAX_EXECUTION_TIME(3500)"),
				"%s / %s:语句超时必须是最外层 SELECT 的第一条提示(MySQL 只认最外层的 MAX_EXECUTION_TIME),实际开头 %.60q", d, p.name, p.with)
			assert.Equal(t, 1, strings.Count(p.with, "MAX_EXECUTION_TIME("), "%s / %s:语句超时只出现一次", d, p.name)
			assert.NotContains(t, p.without, "MAX_EXECUTION_TIME", "%s / %s:超时为 0 时不带这条提示", d, p.name)
			// 带与不带之间只差这一条提示:去掉它(以及因此变空的注释)就得回到不带超时的文本。
			// 先处理"注释里只有它一条"的情形(整个注释消失),再处理"后面还跟着别的提示"的情形;顺序反了前一种会剩一个空注释。
			stripped := strings.Replace(p.with, "SELECT /*+ MAX_EXECUTION_TIME(3500) */ ", "SELECT ", 1)
			stripped = strings.Replace(stripped, "MAX_EXECUTION_TIME(3500) ", "", 1)
			assert.Equal(t, p.without, stripped, "%s / %s:带超时与不带超时只应相差 MAX_EXECUTION_TIME 这一条提示", d, p.name)
		}
		// 不足 1 毫秒按"不带上限"处理:MAX_EXECUTION_TIME(0) 在服务器上的含义是"不限时",写出来只会误导读日志的人。
		sub := newRecommendStatements(d, 500*time.Microsecond)
		assert.Equal(t, without, sub, "%s:不足 1 毫秒的超时应当渲染成不带提示", d)
		neg := newRecommendStatements(d, -time.Second)
		assert.Equal(t, without, neg, "%s:负的超时应当渲染成不带提示", d)
	}
}

func TestRecommendStatements_PlaceholdersMatchArgs(t *testing.T) {
	excludes := [][]uint64{nil, {7}, make([]uint64, 64)}
	for _, d := range sqldialect.All() {
		stmts := newRecommendStatements(d, 3500*time.Millisecond)
		for _, exclude := range excludes {
			query, args := stmts.byMutual(1001, exclude, 20)
			assert.Equal(t, strings.Count(query, "?"), len(args), "%s / RecommendByMutual / exclude %d 个:占位符与参数个数", d, len(exclude))
			// 参数顺序:playerID ×2 → 窗口 W → playerID ×5 → exclude... → limit。
			require.GreaterOrEqual(t, len(args), 9)
			assert.Equal(t, []any{uint64(1001), uint64(1001), int64(RecommendAnchorWindow)}, args[:3], "%s / RecommendByMutual:前三个参数", d)
			assert.Equal(t, int64(20), args[len(args)-1], "%s / RecommendByMutual:最后一个参数是 limit", d)

			query, args = stmts.anchor(1001, exclude, 5005, 20)
			assert.Equal(t, strings.Count(query, "?"), len(args), "%s / recommendAnchor / exclude %d 个:占位符与参数个数", d, len(exclude))
			// 参数顺序:playerID(数共同好友)→ pivot → 窗口 W → playerID ×6 → exclude... → limit。
			require.GreaterOrEqual(t, len(args), 10)
			assert.Equal(t, []any{uint64(1001), uint64(5005), int64(RecommendAnchorWindow)}, args[:3], "%s / recommendAnchor:前三个参数", d)
			assert.Equal(t, int64(20), args[len(args)-1], "%s / recommendAnchor:最后一个参数是 limit", d)
		}
	}
}

func TestRecommendHintsFor_UnknownDialectPanics(t *testing.T) {
	// 方言是封闭枚举;加了新方言却没给提示表时必须在启动期炸,而不是带着没有提示的 SQL 上线。
	unknown := sqldialect.Dialect(len(sqldialect.All())) // 枚举之外的第一个值
	assert.Panics(t, func() { _ = newRecommendStatements(unknown, 0) })
}

func TestRenderRecommendSQL_UnknownSlotPanics(t *testing.T) {
	var exclusion [recommendExclusionCount]string
	assert.Panics(t, func() { _ = renderRecommendSQL("SELECT {{hint.nope}}1", "", "", exclusion) },
		"模板里出现提示表不认识的位置时必须 panic,不能把花括号原样发给数据库")
}
