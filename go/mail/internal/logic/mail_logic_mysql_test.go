package logic

// logic 接真 data.MailRepo 的集成用例(设计 §10 ④ / ⑦ 的说明):假存储只复述 data 层的返回值,
// 下面三件事只有接真库才证明得了 —— 跨两张表的合并分页、未知 claim_state(模拟 M2 的 2 / 4)在 DeleteMails 里的分类、
// 带附件邮件从发件到"领不了、删不掉"的整条链路。门控、库名白名单与拒绝 TiDB 见 internal/mailtest。

import (
	"context"
	"database/sql"
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"mail/internal/constants"
	"mail/internal/data"
	"mail/internal/mailtest"

	mailpb "proto/mail"
)

// realDeps 组装一套接真库的依赖。ids 是号段的替身:按给定顺序发号,用来让两张表的 id 交错。
func realDeps(t *testing.T, ids ...uint64) (*Deps, *sql.DB, context.Context) {
	t.Helper()
	db, ctx := mailtest.OpenMySQL(t, data.Tables())
	repo := data.NewMailRepo(db)
	var next atomic.Int64
	deps := &Deps{
		Config:   testConfig("dev"),
		Repo:     repo,
		Sweeps:   repo,
		Sessions: &fakeSessions{},
		NextID: func(context.Context) (uint64, error) {
			i := next.Add(1) - 1
			require.Less(t, int(i), len(ids), "号段替身发完了")
			return ids[i], nil
		},
		Now: func() time.Time { return testNow },
	}
	return deps, db, ctx
}

func adminCtx() context.Context { return context.Background() }

func sendPersonalOK(t *testing.T, l *AdminLogic, requestID, to uint64, att *mailpb.MailAttachments) uint64 {
	t.Helper()
	req := validPersonal()
	req.RequestId, req.RecipientPlayerId, req.Attachments = requestID, to, att
	resp, err := l.SendPersonalMail(adminCtx(), req)
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	return resp.GetMailId()
}

func sendSystemOK(t *testing.T, l *AdminLogic, requestID uint64, att *mailpb.MailAttachments) uint64 {
	t.Helper()
	req := validSystem()
	req.RequestId, req.Attachments = requestID, att
	resp, err := l.SendSystemMail(adminCtx(), req)
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	return resp.GetSysMailId()
}

// 定向 1 / 3 / 5、系统 2 / 4 / 6 交错:每页 2 封,三页依次是 [6,5] [4,3] [2,1],最后一页 has_more=false;红点对全集算。
func TestListMailsPaginatesAcrossTablesOnRealRepo(t *testing.T) {
	deps, _, _ := realDeps(t, 1, 2, 3, 4, 5, 6)
	admin, client := NewAdminLogic(deps), NewMailLogic(deps)
	for i := uint64(1); i <= 6; i++ {
		if i%2 == 1 {
			assert.Equal(t, i, sendPersonalOK(t, admin, 100+i, 42, nil))
		} else {
			assert.Equal(t, i, sendSystemOK(t, admin, 100+i, nil))
		}
	}

	var before uint64
	for page, want := range [][]uint64{{6, 5}, {4, 3}, {2, 1}} {
		resp, err := client.ListMails(playerCtx(42), &mailpb.ListMailsRequest{BeforeMailId: before, PageSize: 2})
		require.NoError(t, err)
		require.Nil(t, resp.GetErrorMessage())
		assert.Equal(t, want, mailIDs(resp.GetMails()), "第 %d 页", page+1)
		assert.Equal(t, page < 2, resp.GetHasMore(), "第 %d 页 has_more", page+1)
		assert.Equal(t, uint32(6), resp.GetUnreadCount(), "红点对全集算,与页无关")
		before = want[len(want)-1]
	}

	// 别人只看得到系统邮件。
	resp, err := client.ListMails(playerCtx(43), &mailpb.ListMailsRequest{PageSize: 10})
	require.NoError(t, err)
	assert.Equal(t, []uint64{6, 4, 2}, mailIDs(resp.GetMails()))
}

// 未知 claim_state(2 / 4,模拟 M2 的 CLAIMING / NEEDS_MANUAL)即使已过期也进 protected_mail_ids;
// claim_state = 0 的删掉;别人的 / 不存在的 id 算成功、不计数。
func TestDeleteMailsProtectsUnknownClaimStatesOnRealRepo(t *testing.T) {
	deps, db, ctx := realDeps(t)
	for _, row := range []struct {
		id    uint64
		owner uint64
		claim uint32
	}{{1, 42, 0}, {2, 42, 2}, {3, 42, 4}, {4, 43, 0}} {
		_, err := db.ExecContext(ctx,
			`INSERT INTO mail_personal (mail_id, owner_player_id, sender_kind, request_id, request_digest, sender_name, title, body,
			   attachments, claim_state, sent_ms, expire_ms, read_ms, updated_ms, deleted_ms)
			 VALUES (?, ?, 1, ?, 'd', 's', 't', 'b', '', ?, 1, ?, 0, 1, 0)`,
			row.id, row.owner, 1000+row.id, row.claim, testNowMs-dayMs)
		require.NoError(t, err)
	}
	resp, err := NewMailLogic(deps).DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: []uint64{1, 2, 3, 4, 999}})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, uint32(1), resp.GetDeletedCount())
	assert.ElementsMatch(t, []uint64{2, 3}, resp.GetProtectedMailIds())
}

// 带附件的定向邮件整条链路(冒烟 ⑦ 的服务端部分):UNCLAIMED、claimable=1 → 领取短路 → 仍是 UNCLAIMED → 删不掉;
// 别人读它 = NotFound;读了之后未读数减 1。
func TestAttachmentMailLifecycleOnRealRepo(t *testing.T) {
	deps, _, _ := realDeps(t, 10, 11)
	admin, client := NewAdminLogic(deps), NewMailLogic(deps)
	withAtt := sendPersonalOK(t, admin, 1, 42, oneCurrency())
	plain := sendPersonalOK(t, admin, 2, 42, nil)

	list, err := client.ListMails(playerCtx(42), &mailpb.ListMailsRequest{})
	require.NoError(t, err)
	require.Len(t, list.GetMails(), 2)
	assert.Equal(t, uint32(2), list.GetUnreadCount())
	assert.Equal(t, uint32(1), list.GetClaimableCount())
	for _, m := range list.GetMails() {
		if m.GetMailId() == withAtt {
			assert.Equal(t, mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED, m.GetClaimState())
			require.Len(t, m.GetAttachments().GetCurrencies(), 1)
		}
	}

	claim, err := client.ClaimMailAttachments(playerCtx(42), &mailpb.ClaimMailAttachmentsRequest{All: true})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrClaimNotOpen, claim.GetErrorMessage().GetId())

	other, err := client.ReadMail(playerCtx(43), &mailpb.ReadMailRequest{MailId: withAtt})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrNotFound, other.GetErrorMessage().GetId(), "读别人的信 = NotFound")

	read, err := client.ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: withAtt})
	require.NoError(t, err)
	require.Nil(t, read.GetErrorMessage())
	assert.Equal(t, mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED, read.GetSummary().GetClaimState(), "领取短路不改状态")

	del, err := client.DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: []uint64{withAtt, plain}})
	require.NoError(t, err)
	assert.Equal(t, uint32(1), del.GetDeletedCount())
	assert.Equal(t, []uint64{withAtt}, del.GetProtectedMailIds())

	again, err := client.DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: []uint64{plain}})
	require.NoError(t, err)
	assert.Nil(t, again.GetErrorMessage(), "删已删的 = 成功")
	assert.Zero(t, again.GetDeletedCount())

	list, err = client.ListMails(playerCtx(42), &mailpb.ListMailsRequest{})
	require.NoError(t, err)
	assert.Equal(t, []uint64{withAtt}, mailIDs(list.GetMails()))
	assert.Zero(t, list.GetUnreadCount(), "读过的那封不再计未读")
}

// 带附件的系统邮件:物化前后 claim_state 都是 UNCLAIMED(回修 ⑦),删不掉。
func TestSystemAttachmentMailOnRealRepo(t *testing.T) {
	deps, _, _ := realDeps(t, 20)
	admin, client := NewAdminLogic(deps), NewMailLogic(deps)
	sys := sendSystemOK(t, admin, 1, oneCurrency())

	read, err := client.ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: sys})
	require.NoError(t, err)
	require.Nil(t, read.GetErrorMessage())
	assert.Equal(t, mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED, read.GetSummary().GetClaimState())

	del, err := client.DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: []uint64{sys}})
	require.NoError(t, err)
	assert.Equal(t, []uint64{sys}, del.GetProtectedMailIds())
	delRead, err := client.DeleteReadMails(playerCtx(42), &mailpb.DeleteReadMailsRequest{})
	require.NoError(t, err)
	assert.Zero(t, delRead.GetDeletedCount(), "已读但带未领附件的系统邮件不进清理已读")
}
