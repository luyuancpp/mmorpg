package data

// 请求截止 / 取消之后不再提交(docs/design/client-request-ordering.md §6.1 的 guild 行,O11;真库,GUILD_TEST_MYSQL_DSN 未设即 Skip)。
//
// gate 路由 lane 在「观察到完成」(含路由服按超时回 1003)之后才放行同一会话的下一条请求。那一刻 guild 必须已经不会再提交:
// guild 的写事务全部经 inTx,事务 ctx 由请求 ctx 派生(整请求业务预算 = Timeout − 500ms,再套事务子预算),
// database/sql 的 Tx.Commit 先看事务 ctx,已结束就不发 COMMIT(回 ctx 错误或 sql.ErrTxDone),连接上的事务随即回滚。
// 这两条用例钉住的就是这条链在 inTx 上真的成立:写语句已经发出、COMMIT 之前请求结束 → 库里没有这笔写,错误原样回给调用方
// (不伪装成写冲突,也不重跑)。截止之前已经发出的那一次 COMMIT 撤不回来,那一段由业务栅栏兜(§6.4 expected_guild_id)。
//
// 选中本文件:`go test ./internal/data -run TestInTxNoCommitAfter -count=1 -v`(看到 PASS 而不是 SKIP 才算验过)。

import (
	"context"
	"database/sql"
	"errors"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"guild/internal/constants"
)

// TestInTxNoCommitAfterRequestCanceled:写已发出,COMMIT 之前请求 ctx 被取消(上游放弃 / 客户端断开)。
// 取消发生在闭包里、返回之前,所以 Commit 看到的 ctx 一定已结束 —— 结局确定,不依赖调度。
func TestInTxNoCommitAfterRequestCanceled(t *testing.T) {
	ctx, db, repo := openGuildIntegrationRepo(t)
	const (
		guildID uint64 = 7501
		leader  uint64 = 8501
		member  uint64 = 8502
	)
	seedManagedGuild(t, ctx, db, guildID, 1, 1, 50, leader, map[uint64]uint32{member: constants.RoleMember})

	reqCtx, cancel := context.WithCancel(ctx)
	defer cancel()
	attempts := 0
	err := repo.inTx(reqCtx, opLeave, func(ctx context.Context, tx *sql.Tx) error {
		attempts++
		if _, err := tx.ExecContext(ctx, sqlDeleteMember, guildID, member); err != nil {
			return err
		}
		cancel()
		return nil
	})

	require.Error(t, err, "请求已取消还报成功,等于告诉调用方「写成了」而库里其实没有")
	assert.True(t, errors.Is(err, context.Canceled) || errors.Is(err, sql.ErrTxDone), "错误要原样带出取消: %v", err)
	assert.NotErrorIs(t, err, ErrWriteConflict, "调用方自己走了,不能伪装成「稍后重试」")
	assert.Equal(t, 1, attempts, "取消不是可重跑的错误")
	_, found := memberRoleOf(t, ctx, db, guildID, member)
	assert.True(t, found, "COMMIT 之前请求已结束:删掉的成员行必须随回滚回来")
}

// TestInTxNoCommitAfterRequestDeadline:写已发出,COMMIT 之前请求 ctx 的截止到了(对应 gate 侧的后端调用已超时)。
// 截止 200ms 远小于事务子预算 txBudget(1500ms),所以先到的是请求截止,而不是子预算:inTx 必须原样回截止错误,
// 不能按「子预算用完、请求还活着」归成 ErrWriteConflict(那会让客户端在请求已被放弃之后还被要求重试)。
func TestInTxNoCommitAfterRequestDeadline(t *testing.T) {
	ctx, db, repo := openGuildIntegrationRepo(t)
	const (
		guildID uint64 = 7502
		leader  uint64 = 8503
		member  uint64 = 8504
	)
	seedManagedGuild(t, ctx, db, guildID, 1, 1, 50, leader, map[uint64]uint32{member: constants.RoleMember})
	require.True(t, txBudgetFor(opLeave) > 200*time.Millisecond, "前提:请求截止先于事务子预算")

	reqCtx, cancel := context.WithTimeout(ctx, 200*time.Millisecond)
	defer cancel()
	attempts := 0
	err := repo.inTx(reqCtx, opLeave, func(ctx context.Context, tx *sql.Tx) error {
		attempts++
		if _, err := tx.ExecContext(ctx, sqlDeleteMember, guildID, member); err != nil {
			return err
		}
		<-ctx.Done() // 等过请求截止:写已发出,COMMIT 还没发
		return nil
	})

	require.Error(t, err)
	assert.True(t, errors.Is(err, context.DeadlineExceeded) || errors.Is(err, sql.ErrTxDone), "错误要原样带出截止: %v", err)
	assert.NotErrorIs(t, err, ErrWriteConflict, "请求自己的截止到了,不是写冲突")
	assert.Equal(t, 1, attempts)
	_, found := memberRoleOf(t, ctx, db, guildID, member)
	assert.True(t, found, "截止之后不得提交:删掉的成员行必须随回滚回来")
}
