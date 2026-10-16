package logic

// mail_logic.go —— 客户端协议 ClientPlayerMail 的 5 个 C2S 方法(设计 §4.2、§4.3)。
//
// 两类邮件对客户端是同一个 MailSummary,来源只看 kind;mail_id 与 sys_mail_id 出自同一个号段、全局不撞号,
// 所以按 id 定位时先查定向表、再查系统表(都是主键点查),**不信任客户端传 kind**(§3.2)。
//
// 客户端 10 秒超时后会锁住写操作直到一次成功的 ListMails(MailUiState.Tick),所以**所有写方法对同一 id 的
// 重复请求都回成功**(§4.4):已读再读、删不存在 / 已删的信,都是成功。

import (
	"context"
	"fmt"
	"sort"

	"github.com/zeromicro/go-zero/core/logx"

	"mail/internal/constants"
	"mail/internal/data"
	"mail/internal/metrics"
	"mail/internal/session"

	base "proto/common/base"
	mailpb "proto/mail"
)

// 协议上的数量约定(mail.proto 的字段注释是对外的说法,这里是唯一的数值来源)。
const (
	// defaultPageSize / maxPageSize:ListMails 的 page_size 为 0 取默认,超过上限按上限钳制、不报错。
	defaultPageSize uint32 = 20
	maxPageSize     uint32 = 50
	// maxIDsPerRequest:DeleteMails / ClaimMailAttachments 一次最多带多少个 id。
	maxIDsPerRequest = 20
)

// MailLogic 实现客户端的 5 个方法。无状态,整进程共用一个。
type MailLogic struct {
	deps *Deps
}

func NewMailLogic(deps *Deps) *MailLogic {
	return &MailLogic{deps: deps}
}

// requireCaller 取本次请求的权威身份(D-9)。ok=false 只可能是内部调用者直接拨了客户端方法(无会话元数据,
// 拦截器按"内部调用"放行),或会话里 player_id 为 0 —— 都是调用方缺陷,回 kInvalidParameter(不是 fault 码)并打 Error。
func (l *MailLogic) requireCaller(ctx context.Context, method string) (uint64, bool) {
	me, ok := session.ClientPlayerID(ctx)
	if !ok {
		logx.WithContext(ctx).Errorf("[mail] %s missing session identity: method accepts only client requests forwarded through gate -> router (D-9)", method)
	}
	return me, ok
}

// storageFailure 把依赖故障定性成 in-band 的 ErrStorage 并打日志。日志是唯一能看到原始 err 的地方:
// 回给客户端的只有码与一句固定文案(库名、SQL 不出信封,AGENTS §11.3)。
func storageFailure(ctx context.Context, method string, err error) *base.TipInfoMessage {
	logx.WithContext(ctx).Errorf("[mail] %s dependency failure: %v", method, err)
	return tipErr(constants.ErrStorage, "storage unavailable")
}

// ════════════════════════════════════════════════════════════════════════════
// ListMails
// ════════════════════════════════════════════════════════════════════════════

// ListMails 合并定向邮件的一页与系统邮件的有效集,按 mail_id 降序取一页;未读数 / 待领数对**全集**算(§4.2)。
//
// 分页的正确性:定向邮件取 page_size+1 封(mail_id < before 中最大的那些),系统邮件取全部有效且 id < before 的;
// 合并降序后前 page_size 封一定正确 —— 没取到的定向邮件 id 都小于已取到的任何一封定向邮件。
// 合并后多于 page_size 封即 has_more。
func (l *MailLogic) ListMails(ctx context.Context, req *mailpb.ListMailsRequest) (*mailpb.ListMailsResponse, error) {
	const method = "ListMails"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	reject := func(tip *base.TipInfoMessage) (*mailpb.ListMailsResponse, error) {
		return &mailpb.ListMailsResponse{ErrorMessage: tip}, nil
	}
	me, ok := l.requireCaller(ctx, method)
	if !ok {
		return reject(tipErr(constants.ErrInvalidParameter, "missing session identity"))
	}
	pageSize := clampPageSize(req.GetPageSize())
	before := req.GetBeforeMailId()
	now := l.deps.nowMs()

	personal, err := l.deps.Repo.ListPersonal(ctx, me, before, now, pageSize+1)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	unread, claimable, err := l.deps.Repo.PersonalCounts(ctx, me, now)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	systemMails, err := l.visibleSystemMails(ctx, me, now)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}

	merged := make([]*mailpb.MailSummary, 0, len(personal)+len(systemMails))
	for _, rec := range personal {
		summary, err := personalSummary(rec)
		if err != nil {
			return reject(storageFailure(ctx, method, err))
		}
		merged = append(merged, summary)
	}
	for _, summary := range systemMails {
		if !summary.GetRead() {
			unread++
		}
		if summary.GetClaimState() == mailpb.MailClaimState_MAIL_CLAIM_STATE_UNCLAIMED {
			claimable++
		}
		if before == 0 || summary.GetMailId() < before {
			merged = append(merged, summary)
		}
	}
	sort.Slice(merged, func(i, j int) bool { return merged[i].GetMailId() > merged[j].GetMailId() })
	hasMore := uint32(len(merged)) > pageSize
	if hasMore {
		merged = merged[:pageSize]
	}
	return &mailpb.ListMailsResponse{
		Mails:          merged,
		HasMore:        hasMore,
		UnreadCount:    unread,
		ClaimableCount: claimable,
		ServerNowMs:    now,
	}, nil
}

// visibleSystemMails 读对本人可见(有效、且本人没删)的系统邮件:系统有效集(硬上限 MaxActiveSystemMails,截断时 WARN + 计数)并按本人的 state 行过滤、
// 填已读与领取状态。未物化(没有 state 行)的系统邮件:未读,claim_state 按附件推导(§3.3)。
func (l *MailLogic) visibleSystemMails(ctx context.Context, me, now uint64) ([]*mailpb.MailSummary, error) {
	limit := l.deps.Config.Mail.MaxActiveSystemMails
	mails, truncated, err := l.deps.Repo.ListActiveSystem(ctx, now, limit)
	if err != nil {
		return nil, err
	}
	if truncated {
		metrics.ObserveListTruncated()
		logx.WithContext(ctx).Errorf("[mail] WARN active system mails exceed Mail.MaxActiveSystemMails=%d; players only see the newest %d (MailSystemListTruncated)",
			limit, limit)
	}
	if len(mails) == 0 {
		return nil, nil
	}
	ids := make([]uint64, 0, len(mails))
	for _, m := range mails {
		ids = append(ids, m.GetSysMailId())
	}
	states, err := l.deps.Repo.SystemStates(ctx, me, ids)
	if err != nil {
		return nil, err
	}
	out := make([]*mailpb.MailSummary, 0, len(mails))
	for _, m := range mails {
		st := states[m.GetSysMailId()]
		if st != nil && st.GetDeletedMs() > 0 {
			continue
		}
		summary, err := systemSummary(m, st)
		if err != nil {
			return nil, err
		}
		out = append(out, summary)
	}
	return out, nil
}

// clampPageSize:0 取默认,超过上限按上限钳制(协议约定不报错)。
func clampPageSize(n uint32) uint32 {
	switch {
	case n == 0:
		return defaultPageSize
	case n > maxPageSize:
		return maxPageSize
	default:
		return n
	}
}

// ════════════════════════════════════════════════════════════════════════════
// ReadMail
// ════════════════════════════════════════════════════════════════════════════

// ReadMail 读一封信并标已读。不是自己的 / 不存在 / 已删 = ErrNotFound(不区分,不泄露存在性);已过期 = ErrExpired;
// 已读再读 = 成功。过期的系统邮件**不建 state 行**(§4.2"可见只有一个定义")。
func (l *MailLogic) ReadMail(ctx context.Context, req *mailpb.ReadMailRequest) (*mailpb.ReadMailResponse, error) {
	const method = "ReadMail"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	reject := func(tip *base.TipInfoMessage) (*mailpb.ReadMailResponse, error) {
		return &mailpb.ReadMailResponse{ErrorMessage: tip}, nil
	}
	me, ok := l.requireCaller(ctx, method)
	if !ok {
		return reject(tipErr(constants.ErrInvalidParameter, "missing session identity"))
	}
	mailID := req.GetMailId()
	if mailID == 0 {
		return reject(tipErr(constants.ErrInvalidParameter, "mail_id required"))
	}
	now := l.deps.nowMs()

	// ① 定向邮件:条件 UPDATE + 读回。返回 nil = 不是自己的 / 不存在 / 已删 —— 也可能是一封系统邮件,往下查。
	rec, err := l.deps.Repo.MarkPersonalRead(ctx, me, mailID, now)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	if rec != nil {
		if rec.GetExpireMs() <= now {
			return reject(tipErr(constants.ErrExpired, "mail expired"))
		}
		summary, err := personalSummary(rec)
		if err != nil {
			return reject(storageFailure(ctx, method, err))
		}
		return &mailpb.ReadMailResponse{Summary: summary, Body: rec.GetBody(), ServerNowMs: now}, nil
	}

	// ② 系统邮件:先按主键判可见性,只对可见的做条件 upsert(upsert 本身也只认 expire_ms > now,双保险)。
	sys, err := l.deps.Repo.GetSystem(ctx, mailID)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	if sys == nil {
		return reject(tipErr(constants.ErrNotFound, "mail not found"))
	}
	if sys.GetExpireMs() <= now {
		return reject(tipErr(constants.ErrExpired, "mail expired"))
	}
	if _, err := l.deps.Repo.MarkSystemRead(ctx, me, mailID, now); err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	// RowsAffected = 0 既可能是"已读再读",也可能是"本人已删",点查 state 行区分(§4.2,不依赖 clientFoundRows)。
	st, err := l.deps.Repo.GetSystemState(ctx, me, mailID)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	if st == nil {
		// 系统行有效(同一个 now 判过)时条件 upsert 必然插入或命中已有行;没有行只能是存储层出了意料之外的事。
		return reject(storageFailure(ctx, method, fmt.Errorf("system mail %d visible but no state row for player %d after upsert", mailID, me)))
	}
	if st.GetDeletedMs() > 0 {
		return reject(tipErr(constants.ErrNotFound, "mail not found"))
	}
	summary, err := systemSummary(sys, st)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	return &mailpb.ReadMailResponse{Summary: summary, Body: sys.GetBody(), ServerNowMs: now}, nil
}

// ════════════════════════════════════════════════════════════════════════════
// DeleteMails / DeleteReadMails
// ════════════════════════════════════════════════════════════════════════════

// DeleteMails 删一批信(1..20 个 id)。删不存在 / 已删 = 成功、不计数;带未领且未过期附件的信不删,
// 原样回在 protected_mail_ids 里,**整请求仍成功**(M-10)。
//
// 定向邮件一条 UPDATE(白名单在 SQL 里),之后按 id 点查分类;不在定向表(或不是本人的)id 当作可能的系统邮件,
// 逐个做条件 upsert。两段各自自动提交,不需要跨段原子:每个 id 的结果互相独立。
func (l *MailLogic) DeleteMails(ctx context.Context, req *mailpb.DeleteMailsRequest) (*mailpb.DeleteMailsResponse, error) {
	const method = "DeleteMails"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	reject := func(tip *base.TipInfoMessage) (*mailpb.DeleteMailsResponse, error) {
		return &mailpb.DeleteMailsResponse{ErrorMessage: tip}, nil
	}
	me, ok := l.requireCaller(ctx, method)
	if !ok {
		return reject(tipErr(constants.ErrInvalidParameter, "missing session identity"))
	}
	ids, ok := normalizeIDs(req.GetMailIds())
	if !ok || len(ids) == 0 {
		return reject(tipErr(constants.ErrInvalidParameter, fmt.Sprintf("mail_ids must hold 1..%d non-zero ids", maxIDsPerRequest)))
	}
	now := l.deps.nowMs()

	deleted, err := l.deps.Repo.DeletePersonal(ctx, me, ids, now)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	personal, err := l.deps.Repo.PersonalByIDs(ctx, me, ids)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	var protected []uint64
	for _, id := range ids {
		rec, isPersonal := personal[id]
		if isPersonal {
			// UPDATE 之后仍未删 = 白名单没放行 = 受保护(未领且未过期的附件,或 M2 的未终结状态)。
			if rec.GetDeletedMs() == 0 {
				protected = append(protected, id)
			}
			continue
		}
		applied, isProtected, err := l.deleteSystemMail(ctx, me, id, now)
		if err != nil {
			return reject(storageFailure(ctx, method, err))
		}
		if applied {
			deleted++
		}
		if isProtected {
			protected = append(protected, id)
		}
	}
	return &mailpb.DeleteMailsResponse{DeletedCount: uint32(deleted), ProtectedMailIds: protected}, nil
}

// deleteSystemMail 对一个 id 做系统邮件的条件删除并分类结果:
// applied = 本次真删了;protected = 有效、本人未删、但白名单不放行(带附件)。
// 不存在 / 已过期 / 已删过:两者都 false(成功、不计数)。
func (l *MailLogic) deleteSystemMail(ctx context.Context, me, id, now uint64) (applied, protected bool, err error) {
	applied, err = l.deps.Repo.MarkSystemDeleted(ctx, me, id, now)
	if err != nil || applied {
		return applied, false, err
	}
	sys, err := l.deps.Repo.GetSystem(ctx, id)
	if err != nil {
		return false, false, err
	}
	if sys == nil || sys.GetExpireMs() <= now {
		return false, false, nil
	}
	st, err := l.deps.Repo.GetSystemState(ctx, me, id)
	if err != nil {
		return false, false, err
	}
	if st != nil && st.GetDeletedMs() > 0 {
		return false, false, nil
	}
	return false, true, nil
}

// DeleteReadMails 删掉本人全部已读、且白名单放行的信,返回实删数;重复调用 = 成功。
//
// 定向邮件一条带 LIMIT 的 UPDATE(上限 = 邮箱上限;过期未 sweep 的已读行可能更多,下次调用接着删)。
// 系统邮件只看有效集里本人**已读**(有 state 行且 read_ms > 0)、未删、claim_state = 0 的,逐个条件 upsert。
func (l *MailLogic) DeleteReadMails(ctx context.Context, _ *mailpb.DeleteReadMailsRequest) (*mailpb.DeleteReadMailsResponse, error) {
	const method = "DeleteReadMails"
	ctx, cancel := l.deps.withRequestBudget(ctx)
	defer cancel()
	reject := func(tip *base.TipInfoMessage) (*mailpb.DeleteReadMailsResponse, error) {
		return &mailpb.DeleteReadMailsResponse{ErrorMessage: tip}, nil
	}
	me, ok := l.requireCaller(ctx, method)
	if !ok {
		return reject(tipErr(constants.ErrInvalidParameter, "missing session identity"))
	}
	now := l.deps.nowMs()

	deleted, err := l.deps.Repo.DeleteReadPersonal(ctx, me, now, l.deps.Config.Mail.MaxPersonalMailsPerPlayer)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	mails, _, err := l.deps.Repo.ListActiveSystem(ctx, now, l.deps.Config.Mail.MaxActiveSystemMails)
	if err != nil {
		return reject(storageFailure(ctx, method, err))
	}
	if len(mails) > 0 {
		ids := make([]uint64, 0, len(mails))
		for _, m := range mails {
			ids = append(ids, m.GetSysMailId())
		}
		states, err := l.deps.Repo.SystemStates(ctx, me, ids)
		if err != nil {
			return reject(storageFailure(ctx, method, err))
		}
		for _, id := range ids {
			st := states[id]
			if st == nil || st.GetReadMs() == 0 || st.GetDeletedMs() > 0 || st.GetClaimState() != data.ClaimStateNone {
				continue
			}
			applied, err := l.deps.Repo.MarkSystemDeleted(ctx, me, id, now)
			if err != nil {
				return reject(storageFailure(ctx, method, err))
			}
			if applied {
				deleted++
			}
		}
	}
	return &mailpb.DeleteReadMailsResponse{DeletedCount: uint32(deleted)}, nil
}

// ════════════════════════════════════════════════════════════════════════════
// ClaimMailAttachments(M1 短路)
// ════════════════════════════════════════════════════════════════════════════

// ClaimMailAttachments 在 M1 只做身份与形状校验,然后回 ErrClaimNotOpen(设计 §4.3):
// **不读库、不写库、不调号段、不调 scene、不改任何 claim_state**,只计 mail_claim_short_circuit_total。
// mail_logic_test.go 用一个任何调用都会让用例失败的 store 守住"零调用"。
func (l *MailLogic) ClaimMailAttachments(ctx context.Context, req *mailpb.ClaimMailAttachmentsRequest) (*mailpb.ClaimMailAttachmentsResponse, error) {
	const method = "ClaimMailAttachments"
	reject := func(tip *base.TipInfoMessage) (*mailpb.ClaimMailAttachmentsResponse, error) {
		return &mailpb.ClaimMailAttachmentsResponse{ErrorMessage: tip}, nil
	}
	if _, ok := l.requireCaller(ctx, method); !ok {
		return reject(tipErr(constants.ErrInvalidParameter, "missing session identity"))
	}
	ids, ok := normalizeIDs(req.GetMailIds())
	if !ok || req.GetAll() == (len(ids) > 0) {
		return reject(tipErr(constants.ErrInvalidParameter, fmt.Sprintf("set either all=true or 1..%d non-zero mail_ids", maxIDsPerRequest)))
	}
	metrics.ObserveClaimShortCircuit()
	return reject(tipErr(constants.ErrClaimNotOpen, "claiming attachments is not open yet"))
}

// ════════════════════════════════════════════════════════════════════════════
// 小工具
// ════════════════════════════════════════════════════════════════════════════

// normalizeIDs 去重(保持首次出现的顺序),拒绝 0 与超过 maxIDsPerRequest 个 id。空列表返回 (nil, true),由调用方判。
func normalizeIDs(raw []uint64) ([]uint64, bool) {
	if len(raw) > maxIDsPerRequest {
		return nil, false
	}
	seen := make(map[uint64]struct{}, len(raw))
	out := make([]uint64, 0, len(raw))
	for _, id := range raw {
		if id == 0 {
			return nil, false
		}
		if _, dup := seen[id]; dup {
			continue
		}
		seen[id] = struct{}{}
		out = append(out, id)
	}
	return out, true
}

// personalSummary 把一行定向邮件转成客户端摘要。附件解不开是存储层数据损坏(发件时校验过才写入),按故障处理。
func personalSummary(rec *mailpb.MailPersonalRecord) (*mailpb.MailSummary, error) {
	attachments, err := decodeAttachments(rec.GetAttachments())
	if err != nil {
		return nil, fmt.Errorf("personal mail %d: %w", rec.GetMailId(), err)
	}
	return &mailpb.MailSummary{
		MailId:      rec.GetMailId(),
		Kind:        mailpb.MailKind_MAIL_KIND_PERSONAL,
		SenderName:  rec.GetSenderName(),
		Title:       rec.GetTitle(),
		SentMs:      rec.GetSentMs(),
		ExpireMs:    rec.GetExpireMs(),
		Read:        rec.GetReadMs() > 0,
		ClaimState:  mailpb.MailClaimState(rec.GetClaimState()),
		Attachments: attachments,
	}, nil
}

// systemSummary 把一封系统邮件与本人的 state 行(可为 nil = 未物化)转成客户端摘要。
// 未物化时 claim_state 按附件推导,与物化时写入的值同一规则(data.ClaimStateFor)。
func systemSummary(sys *mailpb.MailSystemRecord, st *mailpb.MailSystemStateRecord) (*mailpb.MailSummary, error) {
	attachments, err := decodeAttachments(sys.GetAttachments())
	if err != nil {
		return nil, fmt.Errorf("system mail %d: %w", sys.GetSysMailId(), err)
	}
	claim := data.ClaimStateFor(sys.GetAttachments())
	read := false
	if st != nil {
		claim = st.GetClaimState()
		read = st.GetReadMs() > 0
	}
	return &mailpb.MailSummary{
		MailId:      sys.GetSysMailId(),
		Kind:        mailpb.MailKind_MAIL_KIND_SYSTEM,
		SenderName:  sys.GetSenderName(),
		Title:       sys.GetTitle(),
		SentMs:      sys.GetSentMs(),
		ExpireMs:    sys.GetExpireMs(),
		Read:        read,
		ClaimState:  mailpb.MailClaimState(claim),
		Attachments: attachments,
	}, nil
}
