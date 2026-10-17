package logic

import (
	"context"
	"errors"
	"math"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx/logtest"
	"google.golang.org/protobuf/proto"

	"mail/generated/pb/game"
	"mail/internal/constants"
	"mail/internal/data"

	mailpb "proto/mail"
	plpb "proto/player_locator"
)

func validPersonal() *mailpb.SendPersonalMailRequest {
	return &mailpb.SendPersonalMailRequest{
		SenderKind:        mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL,
		RequestId:         7,
		RecipientPlayerId: 42,
		SenderName:        "运营",
		Title:             "维护补偿",
		Body:              "正文",
		ExpireMs:          testNowMs + 7*dayMs,
	}
}

func validSystem() *mailpb.SendSystemMailRequest {
	return &mailpb.SendSystemMailRequest{
		SenderKind: mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL,
		RequestId:  8,
		SenderName: "运营",
		Title:      "公告",
		Body:       "正文",
		ExpireMs:   testNowMs + 7*dayMs,
	}
}

func oneCurrency() *mailpb.MailAttachments {
	return &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{CurrencyType: 1, Amount: 10}}}
}

// ── ① 形状校验:在取号与触库之前 ─────────────────────────────────────────────
//
// "被拒的请求不消耗号段"由"存储零调用"保证:领号发生在存储层的发件路径里(data.MailRepo.SendPersonal / SendSystem),
// 存储一次都没被调,号就不可能被领。假存储不调 NextID,在这里数 NextID 的调用次数恒为 0,证明不了什么。

func TestSendPersonalShapeRejections(t *testing.T) {
	cases := map[string]func(r *mailpb.SendPersonalMailRequest){
		"sender_kind 未指定": func(r *mailpb.SendPersonalMailRequest) {
			r.SenderKind = mailpb.MailSenderKind_MAIL_SENDER_KIND_UNSPECIFIED
		},
		"sender_kind 未知值": func(r *mailpb.SendPersonalMailRequest) { r.SenderKind = 99 },
		"request_id 为 0":  func(r *mailpb.SendPersonalMailRequest) { r.RequestId = 0 },
		"收件人为 0":          func(r *mailpb.SendPersonalMailRequest) { r.RecipientPlayerId = 0 },
		"署名为空":            func(r *mailpb.SendPersonalMailRequest) { r.SenderName = "" },
		"署名超长":            func(r *mailpb.SendPersonalMailRequest) { r.SenderName = strings.Repeat("署", maxSenderNameRunes+1) },
		"标题为空":            func(r *mailpb.SendPersonalMailRequest) { r.Title = "" },
		"标题超长":            func(r *mailpb.SendPersonalMailRequest) { r.Title = strings.Repeat("题", maxTitleRunes+1) },
		"正文超长":            func(r *mailpb.SendPersonalMailRequest) { r.Body = strings.Repeat("文", maxBodyRunes+1) },
		"expire_ms 为 0":   func(r *mailpb.SendPersonalMailRequest) { r.ExpireMs = 0 },
	}
	for name, mutate := range cases {
		t.Run(name, func(t *testing.T) {
			store := newFakeStore()
			req := validPersonal()
			mutate(req)
			resp, err := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), req)
			require.NoError(t, err)
			assert.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId())
			assert.Empty(t, store.callNames(), "被拒的请求不触库(也就不领号)")
		})
	}
}

// 长度按 Unicode 码点算,不按字节:恰好在上限的中文标题合法。
func TestTextLimitsCountRunes(t *testing.T) {
	store := newFakeStore()
	store.sendPersonalResult = data.SendResult{Outcome: data.SendCreated, MailID: 1}
	req := validPersonal()
	req.SenderName = strings.Repeat("署", maxSenderNameRunes)
	req.Title = strings.Repeat("题", maxTitleRunes)
	req.Body = strings.Repeat("文", maxBodyRunes)
	resp, err := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), req)
	require.NoError(t, err)
	assert.Nil(t, resp.GetErrorMessage())
}

func TestAttachmentRulesRejectAsAttachmentInvalid(t *testing.T) {
	logtest.Discard(t)
	five := &mailpb.MailAttachments{}
	for i := uint32(0); i < 5; i++ {
		five.Currencies = append(five.Currencies, &mailpb.MailCurrency{CurrencyType: i, Amount: 1})
	}
	seventeen := &mailpb.MailAttachments{}
	for i := uint32(1); i <= 17; i++ {
		seventeen.Items = append(seventeen.Items, &mailpb.MailItem{ConfigId: i, Count: 1})
	}
	for name, att := range map[string]*mailpb.MailAttachments{
		"货币超过 4 条":      five,
		"物品超过 16 条":     seventeen,
		"货币类型不在白名单":     {Currencies: []*mailpb.MailCurrency{{CurrencyType: 3, Amount: 1}}},
		"货币类型重复":        {Currencies: []*mailpb.MailCurrency{{CurrencyType: 1, Amount: 1}, {CurrencyType: 1, Amount: 2}}},
		"货币数量为 0":       {Currencies: []*mailpb.MailCurrency{{CurrencyType: 1, Amount: 0}}},
		"货币数量超过 int64":  {Currencies: []*mailpb.MailCurrency{{CurrencyType: 1, Amount: math.MaxInt64 + 1}}},
		"物品 config 为 0": {Items: []*mailpb.MailItem{{ConfigId: 0, Count: 1}}},
		"物品重复":          {Items: []*mailpb.MailItem{{ConfigId: 5, Count: 1}, {ConfigId: 5, Count: 1}}},
		"物品数量为 0":       {Items: []*mailpb.MailItem{{ConfigId: 5, Count: 0}}},
	} {
		t.Run(name, func(t *testing.T) {
			store := newFakeStore()
			req := validPersonal()
			req.Attachments = att
			resp, _ := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), req)
			assert.Equal(t, constants.ErrAttachmentInvalid, resp.GetErrorMessage().GetId())
			assert.Empty(t, store.callNames())
		})
	}
}

// 边界值本身合法:恰好 4 条货币、16 条物品、amount = INT64_MAX。
func TestAttachmentBoundariesAccepted(t *testing.T) {
	att := &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{CurrencyType: 0, Amount: math.MaxInt64}}}
	for i := uint32(1); i <= maxAttachmentItems; i++ {
		att.Items = append(att.Items, &mailpb.MailItem{ConfigId: i, Count: 1})
	}
	got, err := normalizeAttachments(att, testConfig("dev").Mail.AllowsCurrency)
	require.NoError(t, err)
	assert.Len(t, got.GetItems(), maxAttachmentItems)
}

// ── ② Mode 闸门(§10 ⑥b 的四态)──────────────────────────────────────────────

func TestModeGate(t *testing.T) {
	t.Run("pro + 附件 → MailAttachmentNotOpen,不触库不取号", func(t *testing.T) {
		store := newFakeStore()
		deps := testDeps(store, "pro")
		req := validPersonal()
		req.Attachments = oneCurrency()
		resp, _ := NewAdminLogic(deps).SendPersonalMail(context.Background(), req)
		assert.Equal(t, constants.ErrAttachmentNotOpen, resp.GetErrorMessage().GetId())
		assert.Empty(t, store.callNames(), "被拒的请求不触库(也就不领号)")

		sys := validSystem()
		sys.Attachments = oneCurrency()
		sysResp, _ := NewAdminLogic(deps).SendSystemMail(context.Background(), sys)
		assert.Equal(t, constants.ErrAttachmentNotOpen, sysResp.GetErrorMessage().GetId())
		assert.Empty(t, store.callNames())
	})
	t.Run("pro + ROBOT → kInvalidParameter", func(t *testing.T) {
		store := newFakeStore()
		req := validPersonal()
		req.SenderKind = mailpb.MailSenderKind_MAIL_SENDER_KIND_ROBOT
		resp, _ := NewAdminLogic(testDeps(store, "pro")).SendPersonalMail(context.Background(), req)
		assert.Equal(t, constants.ErrInvalidParameter, resp.GetErrorMessage().GetId())
		assert.Empty(t, store.callNames())
	})
	t.Run("dev + 附件 → 交给存储,附件已编码", func(t *testing.T) {
		store := newFakeStore()
		store.sendPersonalResult = data.SendResult{Outcome: data.SendCreated, MailID: 11}
		req := validPersonal()
		req.Attachments = oneCurrency()
		resp, _ := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), req)
		require.Nil(t, resp.GetErrorMessage())
		assert.NotEmpty(t, store.lastPersonalSend.Record.GetAttachments())
		assert.Equal(t, data.ClaimStateUnclaimed, data.ClaimStateFor(store.lastPersonalSend.Record.GetAttachments()))
	})
	t.Run("pro + 无附件 OPS_TOOL → 成功", func(t *testing.T) {
		store := newFakeStore()
		store.sendPersonalResult = data.SendResult{Outcome: data.SendCreated, MailID: 12}
		resp, _ := NewAdminLogic(testDeps(store, "pro")).SendPersonalMail(context.Background(), validPersonal())
		assert.Nil(t, resp.GetErrorMessage())
		assert.Equal(t, uint64(12), resp.GetMailId())
	})
}

// ── ③ 存储结果的映射 ────────────────────────────────────────────────────────

func TestSendPersonalPassesServerSideLimits(t *testing.T) {
	store := newFakeStore()
	store.sendPersonalResult = data.SendResult{Outcome: data.SendCreated, MailID: 11}
	_, err := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), validPersonal())
	require.NoError(t, err)
	in := store.lastPersonalSend
	assert.Equal(t, testNowMs, in.Record.GetSentMs(), "sent_ms 是服务端时刻,不信调用方")
	assert.Equal(t, testNowMs+30*dayMs, in.MaxExpireMs)
	assert.Equal(t, uint32(100), in.BoxLimit)
	assert.Equal(t, uint64(42), in.Record.GetOwnerPlayerId())
	assert.Len(t, in.Record.GetRequestDigest(), 32)
	assert.NotNil(t, in.NextID)
}

func TestSendPersonalOutcomeMapping(t *testing.T) {
	logtest.Discard(t)
	for _, tc := range []struct {
		name        string
		result      data.SendResult
		storeErr    error
		wantTip     uint32
		wantID      uint64
		wantDup     bool
		wantPush    bool
		wantOutcome string
	}{
		{name: "首发", result: data.SendResult{Outcome: data.SendCreated, MailID: 11}, wantID: 11, wantPush: true, wantOutcome: "ok"},
		{name: "重放", result: data.SendResult{Outcome: data.SendDuplicated, MailID: 11}, wantID: 11, wantDup: true, wantOutcome: "duplicated"},
		{name: "同键异内容", result: data.SendResult{Outcome: data.SendConflict}, wantTip: constants.ErrRequestConflict, wantOutcome: "rejected"},
		{name: "过期时间越界", result: data.SendResult{Outcome: data.SendExpireOutOfRange}, wantTip: constants.ErrInvalidParameter, wantOutcome: "rejected"},
		{name: "满箱", result: data.SendResult{Outcome: data.SendBoxFull}, wantTip: constants.ErrRecipientBoxFull, wantOutcome: "rejected"},
		{name: "存储故障", storeErr: errBoom, wantTip: constants.ErrStorage, wantOutcome: "fault"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			store := newFakeStore()
			store.sendPersonalResult = tc.result
			if tc.storeErr != nil {
				store.errOn["SendPersonal"] = tc.storeErr
			}
			deps := testDeps(store, "dev")
			push := &fakePush{}
			deps.Push = push.push
			deps.Sessions = &fakeSessions{sess: onlineSession()}
			labels := map[string]string{"method": "send_personal", "sender_kind": "ops_tool", "outcome": tc.wantOutcome}
			before := counterValue(t, "mail_admin_send_total", labels)

			resp, err := NewAdminLogic(deps).SendPersonalMail(context.Background(), validPersonal())
			require.NoError(t, err, "一律 in-band")
			assert.Equal(t, tc.wantTip, resp.GetErrorMessage().GetId())
			assert.Equal(t, tc.wantID, resp.GetMailId())
			assert.Equal(t, tc.wantDup, resp.GetDuplicated())
			assert.Equal(t, tc.wantPush, push.calls == 1, "只有首发推送;重放 / 拒绝 / 故障都不推")
			assert.Equal(t, before+1, counterValue(t, "mail_admin_send_total", labels))
		})
	}
}

func TestSendSystemOutcomeMappingAndNoPush(t *testing.T) {
	logtest.Discard(t)
	for _, tc := range []struct {
		name    string
		result  data.SendResult
		wantTip uint32
		wantID  uint64
		wantDup bool
	}{
		{name: "首发", result: data.SendResult{Outcome: data.SendCreated, MailID: 21}, wantID: 21},
		{name: "重放", result: data.SendResult{Outcome: data.SendDuplicated, MailID: 21}, wantID: 21, wantDup: true},
		{name: "上限", result: data.SendResult{Outcome: data.SendSystemLimit}, wantTip: constants.ErrSystemMailLimit},
		{name: "冲突", result: data.SendResult{Outcome: data.SendConflict}, wantTip: constants.ErrRequestConflict},
		{name: "过期时间越界", result: data.SendResult{Outcome: data.SendExpireOutOfRange}, wantTip: constants.ErrInvalidParameter},
	} {
		t.Run(tc.name, func(t *testing.T) {
			store := newFakeStore()
			store.sendSystemResult = tc.result
			deps := testDeps(store, "dev")
			push := &fakePush{}
			deps.Push = push.push
			resp, err := NewAdminLogic(deps).SendSystemMail(context.Background(), validSystem())
			require.NoError(t, err)
			assert.Equal(t, tc.wantTip, resp.GetErrorMessage().GetId())
			assert.Equal(t, tc.wantID, resp.GetSysMailId())
			assert.Equal(t, tc.wantDup, resp.GetDuplicated())
			assert.Zero(t, push.calls, "系统邮件不推送(M-15)")
		})
	}
	store := newFakeStore()
	store.sendSystemResult = data.SendResult{Outcome: data.SendCreated, MailID: 21}
	_, _ = NewAdminLogic(testDeps(store, "dev")).SendSystemMail(context.Background(), validSystem())
	assert.Equal(t, uint32(100), store.lastSystemSend.ActiveLimit)
	assert.Equal(t, testNowMs+30*dayMs, store.lastSystemSend.MaxExpireMs)
}

func TestSendWithoutAllocatorIsFault(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	deps := testDeps(store, "dev")
	deps.NextID = nil
	resp, _ := NewAdminLogic(deps).SendPersonalMail(context.Background(), validPersonal())
	assert.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId())
	sysResp, _ := NewAdminLogic(deps).SendSystemMail(context.Background(), validSystem())
	assert.Equal(t, constants.ErrStorage, sysResp.GetErrorMessage().GetId())
	assert.Empty(t, store.callNames())
}

// ── 请求摘要 ────────────────────────────────────────────────────────────────

// 同一请求的摘要稳定;附件顺序不影响摘要;任何内容字段变化都改变摘要。
func TestRequestDigestCanonical(t *testing.T) {
	digestOf := func(mutate func(r *mailpb.SendPersonalMailRequest)) []byte {
		store := newFakeStore()
		store.sendPersonalResult = data.SendResult{Outcome: data.SendDuplicated, MailID: 1}
		req := validPersonal()
		req.Attachments = &mailpb.MailAttachments{
			Currencies: []*mailpb.MailCurrency{{CurrencyType: 2, Amount: 5}, {CurrencyType: 0, Amount: 7}},
			Items:      []*mailpb.MailItem{{ConfigId: 9, Count: 1}, {ConfigId: 3, Count: 2}},
		}
		if mutate != nil {
			mutate(req)
		}
		_, err := NewAdminLogic(testDeps(store, "dev")).SendPersonalMail(context.Background(), req)
		require.NoError(t, err)
		require.True(t, store.called("SendPersonal"))
		return store.lastPersonalSend.Record.GetRequestDigest()
	}
	base := digestOf(nil)
	assert.Equal(t, base, digestOf(nil), "同一请求摘要稳定")
	assert.Equal(t, base, digestOf(func(r *mailpb.SendPersonalMailRequest) {
		r.Attachments.Currencies[0], r.Attachments.Currencies[1] = r.Attachments.Currencies[1], r.Attachments.Currencies[0]
		r.Attachments.Items[0], r.Attachments.Items[1] = r.Attachments.Items[1], r.Attachments.Items[0]
	}), "附件顺序没有业务含义,不影响摘要")
	for name, mutate := range map[string]func(r *mailpb.SendPersonalMailRequest){
		"收件人":  func(r *mailpb.SendPersonalMailRequest) { r.RecipientPlayerId++ },
		"署名":   func(r *mailpb.SendPersonalMailRequest) { r.SenderName = "客服" },
		"标题":   func(r *mailpb.SendPersonalMailRequest) { r.Title = "另一个标题" },
		"正文":   func(r *mailpb.SendPersonalMailRequest) { r.Body = "另一段正文" },
		"过期时间": func(r *mailpb.SendPersonalMailRequest) { r.ExpireMs++ },
		"附件数量": func(r *mailpb.SendPersonalMailRequest) { r.Attachments.Items[0].Count++ },
		"去掉附件": func(r *mailpb.SendPersonalMailRequest) { r.Attachments = nil },
	} {
		assert.NotEqual(t, base, digestOf(mutate), name)
	}
}

// 带长度前缀:字段边界挪动不会拼出同一串。
func TestRequestDigestFieldBoundaries(t *testing.T) {
	a := requestDigest(sendDigestInput{kind: digestKindSystem, senderKind: 1, requestID: 1, title: "ab", body: "c", expireMs: 1})
	b := requestDigest(sendDigestInput{kind: digestKindSystem, senderKind: 1, requestID: 1, title: "a", body: "bc", expireMs: 1})
	assert.NotEqual(t, a, b)
	p := requestDigest(sendDigestInput{kind: digestKindPersonal, senderKind: 1, requestID: 1, title: "a", expireMs: 1})
	s := requestDigest(sendDigestInput{kind: digestKindSystem, senderKind: 1, requestID: 1, title: "a", expireMs: 1})
	assert.NotEqual(t, p, s)
}

// ── 推送 ────────────────────────────────────────────────────────────────────

func onlineSession() *plpb.PlayerSession {
	return &plpb.PlayerSession{
		PlayerId: 42, SessionId: 77, GateId: "3", GateInstanceId: "gate-uuid",
		State: plpb.PlayerSessionState_SESSION_STATE_ONLINE,
	}
}

func TestPushOutcomes(t *testing.T) {
	logtest.Discard(t)
	disconnecting := onlineSession()
	disconnecting.State = plpb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	for _, tc := range []struct {
		name     string
		noPush   bool
		sessions SessionLookup
		pushErr  error
		want     string
		wantCall bool
	}{
		{name: "通道没配", noPush: true, sessions: &fakeSessions{sess: onlineSession()}, want: "error"},
		{name: "读会话失败不是离线", sessions: &fakeSessions{err: errors.New("redis down")}, want: "session_error"},
		{name: "无会话", sessions: &fakeSessions{}, want: "offline"},
		{name: "断线等重连", sessions: &fakeSessions{sess: disconnecting}, want: "offline"},
		{name: "在线", sessions: &fakeSessions{sess: onlineSession()}, want: "ok", wantCall: true},
		{name: "Kafka 写失败", sessions: &fakeSessions{sess: onlineSession()}, pushErr: errors.New("kafka down"), want: "error", wantCall: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			deps := testDeps(newFakeStore(), "dev")
			deps.Sessions = tc.sessions
			push := &fakePush{err: tc.pushErr}
			if !tc.noPush {
				deps.Push = push.push
			}
			labels := map[string]string{"outcome": tc.want}
			before := counterValue(t, "mail_push_total", labels)
			NewAdminLogic(deps).pushNewMail(context.Background(), 42)
			assert.Equal(t, before+1, counterValue(t, "mail_push_total", labels))
			assert.Equal(t, tc.wantCall, push.calls == 1)
			if tc.wantCall {
				assert.Equal(t, uint32(game.ClientPlayerMailNotifyMailEventMessageId), push.messageID)
				assert.Equal(t, uint64(42), push.to.PlayerID)
				assert.Equal(t, uint32(77), push.to.SessionID)
				assert.Equal(t, "3", push.to.GateID)
				assert.Equal(t, "gate-uuid", push.to.GateInstanceID)
				ev := &mailpb.MailEventS2C{}
				require.NoError(t, proto.Unmarshal(push.body, ev))
				assert.Equal(t, mailpb.MailEventReason_MAIL_EVENT_REASON_NEW_MAIL, ev.GetReason())
				assert.Equal(t, testNowMs, ev.GetTsMs())
			}
		})
	}
}

// 推送失败不影响发件结果。
func TestPushFailureDoesNotAffectResult(t *testing.T) {
	logtest.Discard(t)
	store := newFakeStore()
	store.sendPersonalResult = data.SendResult{Outcome: data.SendCreated, MailID: 11}
	deps := testDeps(store, "dev")
	deps.Sessions = &fakeSessions{sess: onlineSession()}
	deps.Push = (&fakePush{err: errors.New("kafka down")}).push
	resp, err := NewAdminLogic(deps).SendPersonalMail(context.Background(), validPersonal())
	require.NoError(t, err)
	assert.Nil(t, resp.GetErrorMessage())
	assert.Equal(t, uint64(11), resp.GetMailId())
}
