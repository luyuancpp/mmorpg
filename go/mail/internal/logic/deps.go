// Package logic 是 mail 的业务层(设计 docs/design/mail-system.md §2、§4)。
//
// # 错误语义(照 friend,本包全部方法一律照此)
//
// 业务结果**一律 in-band**:`return &Resp{ErrorMessage: tipErr(码)}, nil`。gate / 路由服的回包桥接只认成功响应,
// 返回 gRPC status 时 TipInfoMessage 连同文案一起丢掉。MySQL / 号段真故障也走 in-band,用唯一的 fault 码
// constants.ErrStorage 表达并打一条 Error 日志,由 serverbase 定性成 rpc_inband_fault。**本包没有 `return nil, err`**。
// 唯一的例外在 server 层:MailAdmin 的令牌校验回 gRPC PermissionDenied(东西向接口,调用方看得到 status,设计 §2.5)。
//
// # 身份与预算
//
// 客户端方法的身份只从会话取(D-9,请求体里没有 player_id)。每个方法入口先套 config.RequestBudget():
// 本次请求的 MySQL / 号段 / Redis / Kafka 全用这一个 ctx,不让 go-zero 的超时拦截器抢先回 DeadlineExceeded。
//
// # 权威判定都在存储层
//
// 上限、幂等、删除白名单、系统邮件可见性都在 data 层的 SQL 里(守卫锁内的当前读、条件 upsert)。
// **不要**在这里加"先查一下再写"的预检:预检不持锁,两个并发请求都能通过(AGENTS.md §11.3)。
package logic

import (
	"context"
	"time"

	"github.com/zeromicro/go-zero/core/logx"

	"mail/internal/config"
	"mail/internal/data"
	"mail/internal/svc"

	base "proto/common/base"
	mailpb "proto/mail"
	plpb "proto/player_locator"

	"shared/kafkautil"
)

// MailStore 是本层用到的存储能力,由 *data.MailRepo 实现。做成接口是为了让 logic 的分支(先判重后校验的结果映射、
// 短路不触库、存储故障 in-band、推送不影响结果)能用假 store 单测;锁与 SQL 语义的回归在 data 包的真库测试里。
//
// 契约:返回的 error 一律是依赖故障(本层定性为 ErrStorage);业务结果走返回值(SendOutcome、nil 行、applied=false)。
type MailStore interface {
	SendPersonal(ctx context.Context, in data.PersonalSend) (data.SendResult, error)
	SendSystem(ctx context.Context, in data.SystemSend) (data.SendResult, error)

	ListPersonal(ctx context.Context, ownerID, beforeMailID, nowMs uint64, limit uint32) ([]*mailpb.MailPersonalRecord, error)
	PersonalCounts(ctx context.Context, ownerID, nowMs uint64) (unread, claimable uint32, err error)
	ListActiveSystem(ctx context.Context, nowMs uint64, limit uint32) ([]*mailpb.MailSystemRecord, bool, error)
	SystemStates(ctx context.Context, playerID uint64, sysMailIDs []uint64) (map[uint64]*mailpb.MailSystemStateRecord, error)
	GetSystem(ctx context.Context, sysMailID uint64) (*mailpb.MailSystemRecord, error)
	GetSystemState(ctx context.Context, playerID, sysMailID uint64) (*mailpb.MailSystemStateRecord, error)
	PersonalByIDs(ctx context.Context, ownerID uint64, mailIDs []uint64) (map[uint64]*mailpb.MailPersonalRecord, error)

	MarkPersonalRead(ctx context.Context, ownerID, mailID, nowMs uint64) (*mailpb.MailPersonalRecord, error)
	MarkSystemRead(ctx context.Context, playerID, sysMailID, nowMs uint64) (bool, error)
	DeletePersonal(ctx context.Context, ownerID uint64, mailIDs []uint64, nowMs uint64) (int64, error)
	DeleteReadPersonal(ctx context.Context, ownerID, nowMs uint64, limit uint32) (int64, error)
	MarkSystemDeleted(ctx context.Context, playerID, sysMailID, nowMs uint64) (bool, error)
}

// SweepStore 是后台清理用的 SQL 能力(data/sweep_repo.go)。单独一个接口:测请求路径的假 store 不必实现它。
type SweepStore interface {
	Sweep(ctx context.Context, cutoffMs uint64, batchLimit int, deleteRows bool) (data.SweepReport, error)
}

// SessionLookup 读收件人的 player:session:{id}(推送前找所在 gate),由 *data.SessionReader 实现。
// (nil, nil) = 无会话;error = Redis / 解码 / 身份不符,三者都**不是**"离线"(设计 §4.5)。
type SessionLookup interface {
	Lookup(ctx context.Context, playerID uint64) (*plpb.PlayerSession, error)
}

// PushFunc 把一条已序列化的 S2C 消息推给一个玩家的 gate。生产实现是 kafkautil.PushToPlayer 绑上 writer 与 builder;
// nil 表示推送通道没配(Kafka.Brokers 为空)。
type PushFunc func(ctx context.Context, to kafkautil.PlayerGateInfo, messageID uint32, body []byte) error

// 编译期接缝断言:接口为可测性而造,真实现只在 NewDeps 里赋值一次;没有这几行,接口与实现漂移要到装配那一刻才暴露。
var (
	_ MailStore     = (*data.MailRepo)(nil)
	_ SweepStore    = (*data.MailRepo)(nil)
	_ SessionLookup = (*data.SessionReader)(nil)
)

// Deps 是 logic 层的依赖集合,main 装配一次、整进程共用。字段全是可替换的接缝,单测手工构造。
type Deps struct {
	Config   config.Config
	Repo     MailStore
	Sweeps   SweepStore
	Sessions SessionLookup
	// NextID 领 mail_id / sys_mail_id(号段 biz_tag=mail)。
	NextID data.IDAllocator
	// Push 为 nil = 推送通道没配,新邮件只落库不推(计 mail_push_total{outcome="error"},横幅可见)。
	Push PushFunc
	// AdminToken 是 MailAdmin 的共享令牌(config.ResolveAdminToken 的结果);空 = MailAdmin 整体停用。
	// 值绝不进日志、指标、错误文本。
	AdminToken string
	// Now 是时间源;nil 时取 time.Now。单测注入固定时钟。
	Now func() time.Time
}

// NewDeps 由 main 调用。adminToken 由调用方从环境变量解析后传入(不在这里读环境变量,便于单测与横幅共用一个结果)。
func NewDeps(svcCtx *svc.ServiceContext, adminToken string) *Deps {
	repo := data.NewMailRepo(svcCtx.DB)
	deps := &Deps{
		Config:     svcCtx.Config,
		Repo:       repo,
		Sweeps:     repo,
		Sessions:   svcCtx.Sessions,
		AdminToken: adminToken,
		Now:        time.Now,
	}
	if svcCtx.MailIDs != nil {
		deps.NextID = svcCtx.MailIDs.Next
	}
	if svcCtx.KafkaWriter != nil && svcCtx.GateCommandBuilder != nil {
		writer, builder := svcCtx.KafkaWriter, svcCtx.GateCommandBuilder
		deps.Push = func(ctx context.Context, to kafkautil.PlayerGateInfo, messageID uint32, body []byte) error {
			return kafkautil.PushToPlayer(ctx, writer, builder, to, messageID, body)
		}
	}
	return deps
}

// now 取时间源。
func (d *Deps) now() time.Time {
	if d.Now == nil {
		return time.Now()
	}
	return d.Now()
}

// nowMs 是本次请求的"现在"(毫秒)。一个请求里所有依赖"现在"的判定都用同一个值,
// 否则"已过期"的判定与 SQL 里 expire_ms > ? 的条件可能各看到一个时刻、互相矛盾。
func (d *Deps) nowMs() uint64 {
	ms := d.now().UnixMilli()
	if ms < 0 {
		return 0
	}
	return uint64(ms)
}

// withRequestBudget 给一次请求套上整请求业务预算;父 ctx 更早到期时以父 ctx 为准。
// 预算非正只可能是单测手工构造的 Config(Timeout 为 0):不套,打日志(套上去每个用例都会在第一次 I/O 上超时)。
func (d *Deps) withRequestBudget(ctx context.Context) (context.Context, context.CancelFunc) {
	budget := d.Config.RequestBudget()
	if budget <= 0 {
		logx.WithContext(ctx).Errorf("[mail] Non-positive RequestBudget=%v; no budget for this request (check Timeout configuration)", budget)
		return ctx, func() {}
	}
	return context.WithTimeout(ctx, budget)
}

func tipErr(id uint32, msg string) *base.TipInfoMessage {
	return &base.TipInfoMessage{Id: id, Parameters: []string{msg}}
}
