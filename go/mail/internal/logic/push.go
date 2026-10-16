package logic

// push.go —— 新定向邮件的 S2C 提示(设计 §4.5;纪律照 go/friend/internal/logic/push.go)。
//
// # 投递语义:at-most-once,推送只是"去拉"的信号
//
// Kafka gate-cmd_g<N> → gate → TCP 这条链路没有重试、没有回执、没有离线补推。所以:
//   - 事件体只带 reason + ts_ms,**不带邮件内容**(标题也不带:运维可能填得很长,推送体只做信号);
//   - 红点真相是 ListMails 返回的 unread_count,推送丢了客户端下次拉列表自然看到;
//   - 推送**不是**发件成功的一部分:信已落库,推不到只影响及时性。
//
// # 三条纪律
//
//  1. 只在发件事务**提交之后**、事务之外推;只推首次发出(重放 duplicated 不推)。
//  2. 失败只打日志 + 计 mail_push_total{outcome},本文件的函数**不返回 error**,从签名上堵住"顺手 return err"。
//  3. 发件方不是收件人,没有收件人的会话 metadata:先经 SharedRedis 读 player:session:{收件人},只推 ONLINE。
//     读失败记 session_error 而**不是** offline —— 一次协议事故若记成 offline,会表现为"全服都离线"且零异常指标。
//
// 系统(全服)邮件不推(M-15):在线玩家下次拉取时看到。

import (
	"context"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"mail/generated/pb/game"
	"mail/internal/metrics"

	mailpb "proto/mail"
	plpb "proto/player_locator"

	"shared/kafkautil"
)

// pushTimeout 覆盖"读会话 + 组包 + 写 Kafka"一整段,同时受请求 ctx(整请求预算)封顶。
// 1.5s 与 friend 同为推断值:整请求预算 3.5s 减去发件事务在最坏情况下的占用(要等同一收件人守卫行上的并发发件)。
// 从请求 ctx 派生而不是 context.Background():Kafka 卡住时不能把整个请求拖过 go-zero 的超时拦截器,
// 否则信已发出、调用方却拿到 DeadlineExceeded,按"结果未知"重发 —— 幂等能兜住,但白白多一次往返。
const pushTimeout = 1500 * time.Millisecond

// pushNewMail 推一条 NotifyMailEvent{NEW_MAIL} 给收件人。无返回值:一切失败都已就地记录。
func (l *AdminLogic) pushNewMail(ctx context.Context, recipientID uint64) {
	if l.deps.Push == nil {
		// 通道没配(Kafka.Brokers 为空,本地单机联调常态):不 panic,但记 error —— "推送整个关着"必须在指标上看得见。
		logx.WithContext(ctx).Errorf("[mail] WARN push channel not configured (Kafka.Brokers empty); new mail to=%d is persisted but not pushed", recipientID)
		metrics.ObservePush(metrics.PushError)
		return
	}
	if l.deps.Sessions == nil {
		logx.WithContext(ctx).Errorf("[mail] session reader not wired; cannot push new mail to=%d", recipientID)
		metrics.ObservePush(metrics.PushError)
		return
	}

	pushCtx, cancel := context.WithTimeout(ctx, pushTimeout)
	defer cancel()

	sess, err := l.deps.Sessions.Lookup(pushCtx, recipientID)
	if err != nil {
		logx.WithContext(ctx).Errorf("[mail] Failed to read session; dropping new-mail push to=%d: %v", recipientID, err)
		metrics.ObservePush(metrics.PushSessionError)
		return
	}
	// 只推 ONLINE。DISCONNECTING(断线等重连)刻意不推:那一刻 gate 上没有可写的连接,玩家重连后会自己拉列表。
	if sess == nil || sess.GetState() != plpb.PlayerSessionState_SESSION_STATE_ONLINE {
		metrics.ObservePush(metrics.PushOffline)
		return
	}

	body, err := proto.Marshal(&mailpb.MailEventS2C{
		Reason: mailpb.MailEventReason_MAIL_EVENT_REASON_NEW_MAIL,
		// 服务端时钟:客户端用它给红点排序,用本地时间会因时钟漂移乱序。
		TsMs: l.deps.nowMs(),
	})
	if err != nil {
		logx.WithContext(ctx).Errorf("[mail] Failed to serialize MailEventS2C to=%d: %v", recipientID, err)
		metrics.ObservePush(metrics.PushError)
		return
	}
	// kafkautil.PushToPlayer 对空 gate_instance_id fail-closed(防僵尸不变量,AGENTS.md §7 第 2 条)。
	err = l.deps.Push(pushCtx, kafkautil.PlayerGateInfo{
		PlayerID:       recipientID,
		SessionID:      sess.GetSessionId(),
		GateID:         sess.GetGateId(),
		GateInstanceID: sess.GetGateInstanceId(),
	}, uint32(game.ClientPlayerMailNotifyMailEventMessageId), body)
	if err != nil {
		logx.WithContext(ctx).Errorf("[mail] Push failed to=%d gate=%s: %v", recipientID, sess.GetGateId(), err)
		metrics.ObservePush(metrics.PushError)
		return
	}
	metrics.ObservePush(metrics.PushOK)
}
