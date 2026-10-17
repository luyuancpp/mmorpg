package logic

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx/logtest"

	"mail/internal/constants"

	mailpb "proto/mail"
)

// attachmentBytes 编码一份带一条货币的附件(用于"有附件"的行)。
func attachmentBytes(t *testing.T) []byte {
	t.Helper()
	b, err := encodeAttachments(&mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{CurrencyType: 1, Amount: 10}}})
	require.NoError(t, err)
	return b
}

func personalRow(id uint64, readMs uint64, claim uint32, attachments []byte) *mailpb.MailPersonalRecord {
	return &mailpb.MailPersonalRecord{
		MailId: id, OwnerPlayerId: 42, SenderName: "运营", Title: "t", Body: "b",
		Attachments: attachments, ClaimState: claim, SentMs: testNowMs - 10, ExpireMs: testNowMs + 1000, ReadMs: readMs,
	}
}

func systemRow(id uint64, attachments []byte, expireMs uint64) *mailpb.MailSystemRecord {
	return &mailpb.MailSystemRecord{SysMailId: id, SenderName: "运营", Title: "sys", Body: "sb", Attachments: attachments, SentMs: testNowMs - 10, ExpireMs: expireMs}
}

func mailIDs(mails []*mailpb.MailSummary) []uint64 {
	out := make([]uint64, 0, len(mails))
	for _, m := range mails {
		out = append(out, m.GetMailId())
	}
	return out
}

// ── 身份 ────────────────────────────────────────────────────────────────────

// 没有会话身份的调用(内部调用者直接拨了客户端方法)一律 kInvalidParameter,且不碰存储。
func TestClientMethodsRequireSession(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	l := NewMailLogic(testDeps(store, "dev"))
	ctx := context.Background()

	list, _ := l.ListMails(ctx, &mailpb.ListMailsRequest{})
	read, _ := l.ReadMail(ctx, &mailpb.ReadMailRequest{MailId: 1})
	del, _ := l.DeleteMails(ctx, &mailpb.DeleteMailsRequest{MailIds: []uint64{1}})
	delRead, _ := l.DeleteReadMails(ctx, &mailpb.DeleteReadMailsRequest{})
	claim, _ := l.ClaimMailAttachments(ctx, &mailpb.ClaimMailAttachmentsRequest{All: true})
	for name, tip := range map[string]uint32{
		"ListMails": list.GetErrorMessage().GetId(), "ReadMail": read.GetErrorMessage().GetId(),
		"DeleteMails": del.GetErrorMessage().GetId(), "DeleteReadMails": delRead.GetErrorMessage().GetId(),
		"ClaimMailAttachments": claim.GetErrorMessage().GetId(),
	} {
		assert.Equal(t, constants.ErrInvalidParameter, tip, name)
	}
	assert.Empty(t, store.callNames(), "无身份时不得碰存储")
}

// ── ListMails ───────────────────────────────────────────────────────────────

// 合并、排序、分页、红点全集计数:定向 [10, 7],系统 [9(带附件、未物化), 5(本人已删), 3(本人已读)]。
func TestListMailsMergesPaginatesAndCounts(t *testing.T) {
	store := newFakeStore()
	store.personalList = []*mailpb.MailPersonalRecord{personalRow(10, 0, 0, nil), personalRow(7, testNowMs-1, 0, nil)}
	store.unread, store.claimable = 2, 0
	att := attachmentBytes(t)
	store.systemList = []*mailpb.MailSystemRecord{systemRow(9, att, testNowMs+1000), systemRow(5, nil, testNowMs+1000), systemRow(3, nil, testNowMs+1000)}
	store.states[5] = &mailpb.MailSystemStateRecord{PlayerId: 42, SysMailId: 5, DeletedMs: testNowMs - 1}
	store.states[3] = &mailpb.MailSystemStateRecord{PlayerId: 42, SysMailId: 3, ReadMs: testNowMs - 1}

	resp, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{PageSize: 3})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, []uint64{10, 9, 7}, mailIDs(resp.GetMails()), "按 mail_id 降序;本人删掉的系统邮件 5 不出现")
	assert.True(t, resp.GetHasMore(), "合并后 4 封 > page_size 3")
	assert.Equal(t, uint32(3), resp.GetUnreadCount(), "定向 2 + 系统 9(未物化 = 未读);3 已读、5 已删不计")
	assert.Equal(t, uint32(1), resp.GetClaimableCount(), "系统 9 带附件、未物化 → 按附件推导为 UNCLAIMED")
	assert.Equal(t, testNowMs, resp.GetServerNowMs())

	byID := map[uint64]*mailpb.MailSummary{}
	for _, m := range resp.GetMails() {
		byID[m.GetMailId()] = m
	}
	assert.Equal(t, mailpb.MailKind_MAIL_KIND_SYSTEM, byID[9].GetKind())
	assert.Equal(t, mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED, byID[9].GetClaimState())
	assert.Len(t, byID[9].GetAttachments().GetCurrencies(), 1)
	assert.False(t, byID[9].GetRead())
	assert.Equal(t, mailpb.MailKind_MAIL_KIND_PERSONAL, byID[7].GetKind())
	assert.True(t, byID[7].GetRead())
}

func TestListMailsFetchesOneExtraPersonalRow(t *testing.T) {
	store := newFakeStore()
	_, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{PageSize: 3})
	require.NoError(t, err)
	// 定向邮件取 page_size+1 封,多出的一封只用来判 has_more。
	assert.Equal(t, uint32(4), store.lastListLimit)
}

func TestListMailsPageSizeClamp(t *testing.T) {
	for _, tc := range []struct{ in, wantLimit uint32 }{{0, defaultPageSize + 1}, {1, 2}, {50, 51}, {500, maxPageSize + 1}} {
		store := newFakeStore()
		_, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{PageSize: tc.in})
		require.NoError(t, err)
		assert.Equal(t, tc.wantLimit, store.lastListLimit, "page_size=%d", tc.in)
	}
}

// 游标同样作用于系统邮件;红点计数仍对全集。
func TestListMailsCursorAppliesToSystemMails(t *testing.T) {
	store := newFakeStore()
	store.systemList = []*mailpb.MailSystemRecord{systemRow(9, nil, testNowMs+1000), systemRow(4, nil, testNowMs+1000)}
	resp, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{BeforeMailId: 8})
	require.NoError(t, err)
	assert.Equal(t, []uint64{4}, mailIDs(resp.GetMails()))
	assert.Equal(t, uint64(8), store.lastListBefore)
	assert.Equal(t, uint32(2), resp.GetUnreadCount(), "红点对全集算,不受游标影响")
	assert.False(t, resp.GetHasMore())
}

func TestListMailsTruncationIsObservable(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	store.systemTruncated = true
	before := counterValue(t, "mail_list_truncated_total", nil)
	_, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{})
	require.NoError(t, err)
	assert.Equal(t, before+1, counterValue(t, "mail_list_truncated_total", nil))
	assert.Equal(t, uint32(100), store.lastSystemLimit, "系统有效集的 LIMIT = Mail.MaxActiveSystemMails")
}

func TestListMailsStorageFailureIsInBand(t *testing.T) {
	logtest.Discard(t)
	for _, method := range []string{"ListPersonal", "PersonalCounts", "ListActiveSystem", "SystemStates"} {
		store := newFakeStore()
		store.systemList = []*mailpb.MailSystemRecord{systemRow(9, nil, testNowMs+1000)}
		store.errOn[method] = errBoom
		resp, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{})
		require.NoError(t, err, "存储故障也走 in-band")
		assert.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId(), method)
		assert.Empty(t, resp.GetMails())
	}
}

func TestListMailsCorruptAttachmentsIsFault(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	store.personalList = []*mailpb.MailPersonalRecord{personalRow(10, 0, 1, []byte{0xff, 0xff, 0xff})}
	resp, err := NewMailLogic(testDeps(store, "dev")).ListMails(playerCtx(42), &mailpb.ListMailsRequest{})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId(), "附件解不开是数据损坏,按故障告警")
}

// ── ReadMail ────────────────────────────────────────────────────────────────

func TestReadMailPersonal(t *testing.T) {
	store := newFakeStore()
	row := personalRow(10, testNowMs, 0, nil)
	store.personalRead = row
	resp, err := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 10})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, "b", resp.GetBody())
	assert.True(t, resp.GetSummary().GetRead())
	assert.Equal(t, mailpb.MailKind_MAIL_KIND_PERSONAL, resp.GetSummary().GetKind())
	assert.False(t, store.called("GetSystem"), "定向邮件命中后不再查系统表")
}

func TestReadMailPersonalExpired(t *testing.T) {
	store := newFakeStore()
	row := personalRow(10, 0, 0, nil)
	row.ExpireMs = testNowMs
	store.personalRead = row
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 10})
	assert.Equal(t, constants.ErrExpired, resp.GetErrorMessage().GetId())
	assert.Empty(t, resp.GetBody(), "过期的信不回正文")
}

func TestReadMailNotFound(t *testing.T) {
	store := newFakeStore()
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 10})
	assert.Equal(t, constants.ErrNotFound, resp.GetErrorMessage().GetId(), "不是自己的 / 不存在 / 已删 同码")
}

// 回修 ⑥:过期的系统邮件回 MailExpired,且**不发**条件 upsert(不建 state 行)。
func TestReadMailSystemExpiredCreatesNoState(t *testing.T) {
	store := newFakeStore()
	store.systemByID[9] = systemRow(9, nil, testNowMs)
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 9})
	assert.Equal(t, constants.ErrExpired, resp.GetErrorMessage().GetId())
	assert.False(t, store.called("MarkSystemRead"))
}

func TestReadMailSystemDeletedByMe(t *testing.T) {
	store := newFakeStore()
	store.systemByID[9] = systemRow(9, nil, testNowMs+1000)
	store.states[9] = &mailpb.MailSystemStateRecord{PlayerId: 42, SysMailId: 9, DeletedMs: testNowMs - 1}
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 9})
	assert.Equal(t, constants.ErrNotFound, resp.GetErrorMessage().GetId())
}

func TestReadMailSystemMarksRead(t *testing.T) {
	store := newFakeStore()
	store.systemByID[9] = systemRow(9, attachmentBytes(t), testNowMs+1000)
	store.states[9] = &mailpb.MailSystemStateRecord{PlayerId: 42, SysMailId: 9, ClaimState: 1} // upsert 物化出的行
	resp, err := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 9})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, "sb", resp.GetBody())
	assert.True(t, resp.GetSummary().GetRead())
	assert.Equal(t, mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED, resp.GetSummary().GetClaimState())
	assert.Equal(t, mailpb.MailKind_MAIL_KIND_SYSTEM, resp.GetSummary().GetKind())
}

// 系统行有效、upsert 之后却没有 state 行:存储层出了意料之外的事,按故障处理而不是谎报成功。
func TestReadMailSystemMissingStateIsFault(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	store.systemByID[9] = systemRow(9, nil, testNowMs+1000)
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{MailId: 9})
	assert.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId())
}

func TestReadMailRejectsZeroID(t *testing.T) {
	store := newFakeStore()
	resp, _ := NewMailLogic(testDeps(store, "dev")).ReadMail(playerCtx(42), &mailpb.ReadMailRequest{})
	assert.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId())
	assert.Empty(t, store.callNames())
}

// ── DeleteMails ─────────────────────────────────────────────────────────────

func TestDeleteMailsRejectsBadShapes(t *testing.T) {
	tooMany := make([]uint64, maxIDsPerRequest+1)
	for i := range tooMany {
		tooMany[i] = uint64(i + 1)
	}
	for name, ids := range map[string][]uint64{"empty": nil, "zero": {1, 0}, "too many": tooMany} {
		store := newFakeStore()
		resp, _ := NewMailLogic(testDeps(store, "dev")).DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: ids})
		assert.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId(), name)
		assert.Empty(t, store.callNames(), name)
	}
}

// 分类:定向 1 删了;定向 2 受保护;系统 9 删了;系统 8 带附件受保护;系统 7 本人早删过;系统 6 已过期;99 不存在。
func TestDeleteMailsClassifiesEveryID(t *testing.T) {
	store := newFakeStore()
	store.deletePersonalN = 1
	store.personalByIDs[1] = &mailpb.MailPersonalRecord{MailId: 1, DeletedMs: testNowMs}
	store.personalByIDs[2] = &mailpb.MailPersonalRecord{MailId: 2, ClaimState: 1}
	store.systemDeleteResult[9] = true
	store.systemByID[8] = systemRow(8, attachmentBytes(t), testNowMs+1000)
	store.systemByID[7] = systemRow(7, nil, testNowMs+1000)
	store.states[7] = &mailpb.MailSystemStateRecord{SysMailId: 7, DeletedMs: testNowMs - 5}
	store.systemByID[6] = systemRow(6, nil, testNowMs)

	resp, err := NewMailLogic(testDeps(store, "dev")).DeleteMails(playerCtx(42),
		&mailpb.DeleteMailsRequest{MailIds: []uint64{1, 2, 9, 8, 7, 6, 99, 1}})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage(), "受保护的 id 不让整请求失败")
	assert.Equal(t, uint32(2), resp.GetDeletedCount(), "定向 1 + 系统 9")
	assert.Equal(t, []uint64{2, 8}, resp.GetProtectedMailIds())
	assert.Equal(t, []uint64{1, 2, 9, 8, 7, 6, 99}, store.lastDeleteIDs, "id 去重且保持顺序")
	assert.Equal(t, []uint64{9, 8, 7, 6, 99}, store.systemDeleteCalls, "只有不在定向表的 id 才按系统邮件删")
}

func TestDeleteMailsStorageFailure(t *testing.T) {
	logtest.Discard(t)
	for _, method := range []string{"DeletePersonal", "PersonalByIDs", "MarkSystemDeleted"} {
		store := newFakeStore()
		store.errOn[method] = errBoom
		resp, _ := NewMailLogic(testDeps(store, "dev")).DeleteMails(playerCtx(42), &mailpb.DeleteMailsRequest{MailIds: []uint64{9}})
		assert.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId(), method)
	}
}

// ── DeleteReadMails ─────────────────────────────────────────────────────────

// 只删本人已读、未删、claim_state = 0 的系统邮件;定向部分的上限 = 邮箱上限。
func TestDeleteReadMails(t *testing.T) {
	store := newFakeStore()
	store.deleteReadN = 3
	store.systemList = []*mailpb.MailSystemRecord{
		systemRow(9, nil, testNowMs+1000), systemRow(8, nil, testNowMs+1000),
		systemRow(7, attachmentBytes(t), testNowMs+1000), systemRow(6, nil, testNowMs+1000),
	}
	store.states[9] = &mailpb.MailSystemStateRecord{SysMailId: 9, ReadMs: 1}                // 删
	store.states[7] = &mailpb.MailSystemStateRecord{SysMailId: 7, ReadMs: 1, ClaimState: 1} // 带附件:不删
	store.states[6] = &mailpb.MailSystemStateRecord{SysMailId: 6, ReadMs: 1, DeletedMs: 1}  // 已删
	store.systemDeleteResult[9] = true                                                      // 8 未读(无 state):不删

	resp, err := NewMailLogic(testDeps(store, "dev")).DeleteReadMails(playerCtx(42), &mailpb.DeleteReadMailsRequest{})
	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, uint32(4), resp.GetDeletedCount())
	assert.Equal(t, []uint64{9}, store.systemDeleteCalls)
	assert.Equal(t, uint32(100), store.lastDeleteReadCap)
}

// ── ClaimMailAttachments(短路)─────────────────────────────────────────────

// 短路不触库:形状合法的请求恒回 MailClaimNotOpen,存储零调用,只计 mail_claim_short_circuit_total。
func TestClaimShortCircuitTouchesNothing(t *testing.T) {
	store := newFakeStore()
	deps := testDeps(store, "dev")
	idCalls := 0
	deps.NextID = func(context.Context) (uint64, error) { idCalls++; return 1, nil }
	push := &fakePush{}
	deps.Push = push.push
	l := NewMailLogic(deps)

	before := counterValue(t, "mail_claim_short_circuit_total", nil)
	for _, req := range []*mailpb.ClaimMailAttachmentsRequest{{All: true}, {MailIds: []uint64{3, 4}}} {
		resp, err := l.ClaimMailAttachments(playerCtx(42), req)
		require.NoError(t, err)
		assert.Equal(t, constants.ErrClaimNotOpen, resp.GetErrorMessage().GetId())
	}
	assert.Empty(t, store.callNames(), "短路不得读写存储")
	assert.Zero(t, idCalls, "短路不得消耗号段")
	assert.Zero(t, push.calls)
	assert.Equal(t, before+2, counterValue(t, "mail_claim_short_circuit_total", nil))
}

func TestClaimRejectsBadShapes(t *testing.T) {
	tooMany := make([]uint64, maxIDsPerRequest+1)
	for i := range tooMany {
		tooMany[i] = uint64(i + 1)
	}
	for name, req := range map[string]*mailpb.ClaimMailAttachmentsRequest{
		"neither":  {},
		"both":     {All: true, MailIds: []uint64{1}},
		"zero id":  {MailIds: []uint64{0}},
		"too many": {MailIds: tooMany},
	} {
		store := newFakeStore()
		resp, _ := NewMailLogic(testDeps(store, "dev")).ClaimMailAttachments(playerCtx(42), req)
		assert.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId(), name)
		assert.Empty(t, store.callNames(), name)
	}
}
