#include "client_request_lane.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "gate_codec.h"
#include "error_reporter/error_reporter.h"
#include "node/system/grpc_call_deadline.h"
#include "proto/common/base/message.pb.h"
#include "proto/common/base/node.pb.h"
#include "proto/common/base/session.pb.h"
#include "rpc/service_metadata/login_service_metadata.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include <session/manager/session_manager.h>
#include "session/comp/router_lane.h"
#include "session/comp/router_lane_item.h"

namespace gate_lane
{
namespace
{

// 在途索引的值。只存定位与回执所需的数据,不存连接(AGENTS.md §11.7)。
struct InflightCall
{
	SteadyTime expiresAt;
	SessionId sessionId{0};
	uint64_t incarnation{0};
	uint64_t clientRequestId{0};
	uint32_t messageId{0};
	entt::entity routerEntity{entt::null};
};

// 只用于每 30s 一行的 [lane] 汇总日志(gate 没有 Prometheus 出口)。不按会话、玩家或消息号分维度(AGENTS.md §9)。
struct LaneCounters
{
	uint64_t admitted{0};
	uint64_t sent{0};
	uint64_t replied{0};
	uint64_t failed{0};
	uint64_t nodeRemoved{0};
	uint64_t expired{0};
	uint64_t expiredInQueue{0};
	uint64_t noRouter{0};
	uint64_t rejectedFull{0};
	uint64_t rejectedRedirected{0};
	uint64_t lateDropped{0};
	uint64_t incarnationMismatch{0};
	uint64_t droppedOnClose{0};
	uint64_t droppedOnRedirect{0};
	std::size_t maxDepthSeen{0};
};

// 全部只在 gate 唯一的 EventLoop 线程上读写(会话表同为 thread_local,client_message_processor.cpp:542-546)。
thread_local InflightIndex<InflightCall> tlsInflight;
thread_local CallSeq tlsLastCallSeq = kNoCallSeq;
thread_local uint64_t tlsLastIncarnation = 0;
thread_local LaneCounters tlsCounters;
thread_local SteadyTime tlsLastSummaryAt{};
// 启动时注入一次(SetRouterSender),之后只读。
RouterSendFn gRouterSender = nullptr;

constexpr std::chrono::seconds kSummaryInterval{30};

CallSeq NextCallSeq()
{
	return ++tlsLastCallSeq;
}

// 会话尚未绑定玩家时 SessionInfo.playerId 是 kInvalidGuid(UINT64_MAX),Go 各服务只认 0 = 无玩家:
// 原样带出去会被当成玩家 18446744073709551615(client-access-band-routing.md 阶段 0b)。
uint64_t NormalizePlayerId(uint64_t playerId)
{
	return playerId == kInvalidGuid ? 0 : playerId;
}

} // namespace

// 以下两个函数导出给 scene 的 TCP(RPC0)回包路径共用(client_request_lane.h;docs/design/muduo-rpc-channel.md §8.1),
// 定义仍留在这里,紧挨着它们用到的计数。

// 带请求 id 与原消息号的错误信封(与路由服的拒绝信封同形,客户端按 id 对上请求)。连接已释放就不发。
void SendEnvelope(SessionInfo &session, uint64_t clientRequestId, uint32_t messageId, uint32_t tipId)
{
	const auto conn = session.conn.lock();
	if (!conn)
	{
		return;
	}
	MessageContent envelope;
	envelope.set_id(clientRequestId);
	envelope.set_message_id(messageId);
	envelope.mutable_error_message()->set_id(tipId);
	GetGateCodec().send(conn, envelope);
}

// 按 session_id 找会话并核对会话代次:session_id 被新连接复用之后,旧会话迟到的完成不得落到新会话上(F47)。
SessionInfo *FindLiveSession(SessionId sessionId, uint64_t incarnation)
{
	auto &sessions = tlsSessionManager.sessions();
	const auto it = sessions.find(sessionId);
	if (it == sessions.end())
	{
		return nullptr;
	}
	if (it->second.incarnation != incarnation)
	{
		++tlsCounters.incarnationMismatch;
		return nullptr;
	}
	return &it->second;
}

namespace
{

// lane 空闲时按收包顺序发出下一条。排队超时或没有路由服的条目直接回 1003 并继续取下一条;循环次数 ≤ 队长(≤ 32)。
void Pump(SessionId sessionId, SessionInfo &session)
{
	const SteadyTime now = std::chrono::steady_clock::now();
	auto &lane = session.routerLane;
	while (auto item = lane.TakeNextIfIdle())
	{
		if (QueueWaitExceeded(item->enqueuedAt, now, kMaxLaneWait))
		{
			++tlsCounters.expiredInQueue;
			SendEnvelope(session, item->clientRequestId, item->messageId, kServiceUnavailable);
			continue;
		}
		const std::optional<entt::entity> router =
			gRouterSender != nullptr ? gRouterSender(*item->request, item->sessionDetailsBin) : std::nullopt;
		if (!router)
		{
			// 本地没有任何路由服(启动窗口 / 全部下线);发送函数已按 1024 采样留痕。
			++tlsCounters.noRouter;
			SendEnvelope(session, item->clientRequestId, item->messageId, kServiceUnavailable);
			continue;
		}
		lane.MarkSent(item->seq);
		const auto deadline = grpc_call_deadline::Get(eNodeType::ClientRpcRouterNodeService);
		tlsInflight.Insert(item->seq, InflightCall{
										  now + deadline + kInflightExpiryMargin,
										  sessionId,
										  session.incarnation,
										  item->clientRequestId,
										  item->messageId,
										  *router,
									  });
		// 确实发出过 Login.Login 的会话,断开时才需要通知 Login 清理 session-id 状态。在发出时置位、不在入队时:
		// 排队中、断线时作废的 Login 从没到达 login,入队就置位会让「发 Login 后立刻断开」放大断线 RPC。
		if (item->messageId == ClientPlayerLoginLoginMessageId)
		{
			session.loginStarted = true;
		}
		++tlsCounters.sent;
	}
}

// 失败类完成(gRPC 失败 / 节点摘除 / 本地截止)的共同收尾:核对会话、认领队头、回 1003、放行下一条。
void FinishWithFailure(CallSeq seq, const InflightCall &call)
{
	SessionInfo *session = FindLiveSession(call.sessionId, call.incarnation);
	if (session == nullptr)
	{
		return;
	}
	if (!session->routerLane.Complete(seq))
	{
		// 不变量:在途索引里的序号必然是该会话 lane 的在途序号(Insert 只发生在 MarkSent 之后、断线时同步注销)。
		LOG_ERROR << "[lane] In-flight call does not match the session lane head; dropping completion"
				  << " session_id=" << call.sessionId << " seq=" << seq
				  << " lane_in_flight=" << session->routerLane.InFlightSeq();
		return;
	}
	SendEnvelope(*session, call.clientRequestId, call.messageId, kServiceUnavailable);
	Pump(call.sessionId, *session);
}

// 认领一次完成:恰好一次(extract)。取不到 = 已过期 / 已失败 / 已因节点摘除处理 / 断线时已注销,调用方丢弃这次完成。
std::optional<InflightCall> Claim(CallSeq seq)
{
	auto call = tlsInflight.Extract(seq);
	if (!call)
	{
		++tlsCounters.lateDropped;
	}
	return call;
}

void LogSummaryIfDue(SteadyTime now)
{
	if (now - tlsLastSummaryAt < kSummaryInterval)
	{
		return;
	}
	tlsLastSummaryAt = now;
	const auto &c = tlsCounters;
	LOG_INFO << "[lane] summary admitted=" << c.admitted << " sent=" << c.sent << " replied=" << c.replied
			 << " failed=" << c.failed << " node_removed=" << c.nodeRemoved << " expired=" << c.expired
			 << " expired_in_queue=" << c.expiredInQueue << " no_router=" << c.noRouter
			 << " rejected_full=" << c.rejectedFull << " rejected_redirected=" << c.rejectedRedirected
			 << " late_dropped=" << c.lateDropped << " incarnation_mismatch=" << c.incarnationMismatch
			 << " dropped_on_close=" << c.droppedOnClose << " dropped_on_redirect=" << c.droppedOnRedirect
			 << " in_flight=" << tlsInflight.Size() << " expiry_heap=" << tlsInflight.ExpiryHeapSize()
			 << " max_depth_seen=" << c.maxDepthSeen;
}

} // namespace

uint64_t NextIncarnation()
{
	return ++tlsLastIncarnation;
}

void SetRouterSender(RouterSendFn fn)
{
	gRouterSender = fn;
}

void Admit(SessionId sessionId, SessionInfo &session, const std::shared_ptr<ClientRequest> &request,
		   SessionDetails details)
{
	const uint64_t clientRequestId = request->id();
	const uint32_t messageId = request->message_id();

	details.set_player_id(NormalizePlayerId(details.player_id()));
	details.set_session_incarnation(session.incarnation);
	const CallSeq seq = NextCallSeq();
	details.set_gate_call_seq(seq);

	RouterLaneItem item;
	item.request = request;
	item.sessionDetailsBin = details.SerializeAsString();
	item.seq = seq;
	item.enqueuedAt = std::chrono::steady_clock::now();
	item.clientRequestId = clientRequestId;
	item.messageId = messageId;

	switch (session.routerLane.Admit(std::move(item), kMaxLaneDepth))
	{
	case AdmitResult::kQueued:
		break;
	case AdmitResult::kRejectedFull:
		// 正常客户端按功能单飞,排到 32 条只可能是在刷;不计非法包(超限本身不是协议错误),但要留趋势证据。
		++tlsCounters.rejectedFull;
		if ((tlsCounters.rejectedFull & 0x3FF) == 1)
		{
			LOG_WARN << "[lane] Session router lane is full; rejecting with 1008 (sampled)"
					 << " latest_session_id=" << sessionId << " depth_limit=" << kMaxLaneDepth
					 << " rejected_full_total=" << tlsCounters.rejectedFull;
		}
		SendEnvelope(session, clientRequestId, messageId, kRateLimitExceeded);
		return;
	case AdmitResult::kRejectedRedirected:
		++tlsCounters.rejectedRedirected;
		SendEnvelope(session, clientRequestId, messageId, kServiceUnavailable);
		return;
	}
	++tlsCounters.admitted;
	tlsCounters.maxDepthSeen = (std::max)(tlsCounters.maxDepthSeen, session.routerLane.Depth());
	Pump(sessionId, session);
}

bool OnRouterReply(const SessionDetails &echoed, const ::google::protobuf::Message &reply)
{
	const CallSeq seq = echoed.gate_call_seq();
	if (seq == kNoCallSeq)
	{
		return false;
	}
	const auto call = Claim(seq);
	if (!call)
	{
		// 这个请求已经有过回执(本地截止 / 失败 / 节点摘除),再下发就是同一请求两个回执。
		return true;
	}
	SessionInfo *session = FindLiveSession(call->sessionId, call->incarnation);
	if (session == nullptr)
	{
		return true;
	}
	if (!session->routerLane.Complete(seq))
	{
		LOG_ERROR << "[lane] Reply does not match the session lane head; dropping"
				  << " session_id=" << call->sessionId << " seq=" << seq
				  << " lane_in_flight=" << session->routerLane.InFlightSeq();
		return true;
	}
	++tlsCounters.replied;
	if (const auto conn = session->conn.lock())
	{
		GetGateCodec().send(conn, reply);
	}
	Pump(call->sessionId, *session);
	return true;
}

bool OnRouterCallFailed(const SessionDetails &sent)
{
	const CallSeq seq = sent.gate_call_seq();
	if (seq == kNoCallSeq)
	{
		return false;
	}
	const auto call = Claim(seq);
	if (!call)
	{
		return true;
	}
	++tlsCounters.failed;
	FinishWithFailure(seq, *call);
	return true;
}

void OnRouterNodeRemoved(entt::entity routerEntity)
{
	auto removed = tlsInflight.ExtractIf([routerEntity](const InflightCall &call) { return call.routerEntity == routerEntity; });
	if (removed.empty())
	{
		return;
	}
	tlsCounters.nodeRemoved += removed.size();
	LOG_WARN << "[lane] Router node removed with in-flight calls; failing them with 1003 so their sessions can proceed"
			 << " router_entity=" << entt::to_integral(routerEntity) << " in_flight=" << removed.size();
	// 摘出(恰好一次)同步做,回执与 Pump 推到下一轮:本回调在 DestroyEntity 之前触发(node.cpp OnNodeRemoveEvent),
	// 被摘实体的 NodeInfo / 通道都还在,同步 Pump 会让 PickRandomNode 以 1/N 概率把下一条又发给它。
	// 中间这一轮 lane 的在途序号保持不变,新请求只会排队;会话断线 / 换代 / 重定向由 FindLiveSession 与 Complete(seq) 兜住。
	// 闭包只捕获数据(AGENTS.md §11.7)。
	auto finish = [removed = std::move(removed)]()
	{
		for (const auto &[seq, call] : removed)
		{
			FinishWithFailure(seq, call);
		}
	};
	if (auto *loop = muduo::net::EventLoop::getEventLoopOfCurrentThread())
	{
		loop->queueInLoop(std::move(finish));
		return;
	}
	finish(); // 无 loop 的宿主(单测):同步收尾
}

void ExpireDue(std::chrono::steady_clock::time_point now)
{
	auto expired = tlsInflight.ExtractExpired(now);
	if (!expired.empty())
	{
		tlsCounters.expired += expired.size();
		// 本地截止到期说明 gRPC 回调丢了(节点摘除宽限前的 F29、CQ 轮询 !ok),正常运行时不该出现。
		std::ostringstream message;
		message << "in_flight_expired=" << expired.size() << " total=" << tlsCounters.expired;
		error_reporter::Record(kServiceUnavailable, "router_lane_expired", message.str());
		LOG_WARN << "[lane] In-flight router calls passed their local deadline without any callback; failing them with 1003"
				 << " count=" << expired.size() << " expired_total=" << tlsCounters.expired;
		for (const auto &[seq, call] : expired)
		{
			FinishWithFailure(seq, call);
		}
	}
	LogSummaryIfDue(now);
}

void OnSessionClosing(SessionInfo &session)
{
	auto &lane = session.routerLane;
	if (lane.InFlightSeq() != kNoCallSeq)
	{
		tlsInflight.Erase(lane.InFlightSeq());
	}
	tlsCounters.droppedOnClose += lane.DropQueued();
}

void OnSessionRedirected(SessionInfo &session)
{
	tlsCounters.droppedOnRedirect += session.routerLane.MarkRedirected();
}

} // namespace gate_lane
