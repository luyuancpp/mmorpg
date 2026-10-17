#pragma once

// 玩家持久收件箱的 scene 消费端(docs/design/player-inbox-service.md §7)。
//
// 一句话:收件箱服务(go/player_inbox)为每个 (玩家, 域) 存一条按 seq 升序的待办流;本系统在玩家的 owner scene 上
// 主动拉取(FetchItems)、按 seq 逐条交给执行器、把结局写进与货币同记录的账本(player_database.inbox_ledger,
// 与效果同一次落盘,IB-3)、**落盘之后**再确认(AckItems)。重复投递按 (seq, request_id, msg_type) 核对一致后
// 回放原结局,不一致 fail-closed(IB-9)。
//
// 触发入口(设计 §7.3),全部只是"安排一次检查",真正的拉取在 Tick 里做:
//   * OnPlayerLanded        PlayerLandingSystem::OnEnteredScene 一行调用。实体在本节点新建、或 owner_epoch 变了 =
//                           新落地,无条件拉一次;同节点换图、gate 补发的重复路由直接跳过。**唯一**创建运行时组件的地方。
//   * OnNotifyEvent         PlayerInboxNotifyEvent(Kafka scene-cmd)的生成 handler 守护段一行调用。只是提醒:
//                           找不到实体 / 实体还没落地就丢弃,下次落地会拉。
//   * OnBattleFreezeCleared PlayerBattleSystem 摘 InBattleComp 的唯一入口里一行调用。之前因战斗延后的,立即重查
//                           (不合并、不去抖)。
//   * Tick                  scene main 每秒调一次:到期的检查(单个 tick ≤ 200 次)、在线有界兜底、等落盘与补确认、
//                           在途超时。
//
// 契约:
//   * **线程**:全部入口只在 scene 的 loop 线程上调用(落地钩子、tick、Kafka 事件、gRPC 完成回调都在这个线程,
//     设计 §10)。状态在 thread_local 的 ECS 组件里,没有锁。
//   * **回调只认 player_id**:gRPC 回包与 PING 回调都按 player_id 重新找实体,再核对 domain 与 correlation_id
//     (节点级单调计数器);对不上就丢弃。不捕获实体句柄。
//   * **不阻断进场**(IB-7):收件箱不进 DependencyGate;不可用时只退避(1s→60s,UNIMPLEMENTED 10 分钟)。
//   * **前置闸**(设计 §7.5)在拉取前与应用前各判一次:consumer_enabled、未停用、可改(player_mutability.h)、
//     账本有效、域规格要求时不在战斗中、有 READY 节点。**确认不受前置闸限制**:Ack 只报告已落盘的事实。
//   * **单飞**:同一 (玩家, 域) 同一时刻至多一个在途请求;上一批全部确认之前不拉下一批(设计 §7.4)。
//   * **协议异常即停用**(halt):本实体不再为该域拉取,直到实体下次在本节点新建。不重试、不空转。
//
// 回包适配在 cpp/nodes/scene/rpc_replies/player_inbox_response_handler.cpp(InitPlayerInboxReply)。

#include <cstdint>
#include <string>

#include "entt/src/entt/entity/entity.hpp"

namespace player_inbox
{
	class FetchPlayerInboxRequest;
	class FetchPlayerInboxResponse;
	class AckPlayerInboxRequest;
	class AckPlayerInboxResponse;
} // namespace player_inbox

class PlayerInboxSystem
{
public:
	// Tick 的周期(秒)。scene main 用它注册定时器。
	static constexpr double kTickIntervalSec = 1.0;

	// 启动时调一次(Node 构造之后、配置已加载):注册表自检,失败则本节点停用消费(不阻断启动,IB-7),
	// 并打一行生效配置。
	static void Initialize();

	// ── 触发入口 ──
	static void OnPlayerLanded(entt::entity player);
	static void OnBattleFreezeCleared(entt::entity player);
	static void OnNotifyEvent(uint64_t playerId, uint32_t domain);
	static void Tick();

	// ── gRPC 回包(由 InitPlayerInboxReply 装的处理器转进来)──
	// grpcCode 是线上 grpc-status 的数值(grpc_call_tag.h 的 kGrpcStatus*);非 OK = 结果未知(GrpcCallFailure 的语义)。
	static void OnFetchReply(const player_inbox::FetchPlayerInboxResponse &response);
	static void OnFetchFailed(const player_inbox::FetchPlayerInboxRequest &request, uint32_t grpcCode,
							  const std::string &detail);
	static void OnAckReply(const player_inbox::AckPlayerInboxResponse &response);
	static void OnAckFailed(const player_inbox::AckPlayerInboxRequest &request, uint32_t grpcCode,
							const std::string &detail);

	// ── 测试注入点(AGENTS §11.2 显式依赖:存盘、时钟、传输都要能替换)──
	// 全部传 nullptr 恢复默认实现。生产代码**不调用**这些 setter。

	// 默认 &PlayerLifecycleSystem::SavePlayerToRedis。true = 确实压了一次写;false 不代表任何具体原因,
	// 是否已落盘一律只看 PlayerLastPersistedSnapshotComp。
	using PersistFn = bool (*)(entt::entity);
	static void SetPersistFnForTest(PersistFn fn);

	// 默认单调时钟毫秒(steady_clock)。进程内的退避、去抖、超时都用它(AGENTS §11.3)。
	using NowMsFn = uint64_t (*)();
	static void SetNowFnForTest(NowMsFn fn);

	// 默认:挑一个 READY 的 PlayerInboxNodeService 节点,用生成的 gRPC 客户端发出;返回 false = 没有可用节点。
	using SendFetchFn = bool (*)(const player_inbox::FetchPlayerInboxRequest &);
	using SendAckFn = bool (*)(const player_inbox::AckPlayerInboxRequest &);
	static void SetTransportForTest(SendFetchFn sendFetch, SendAckFn sendAck);

	// 默认读 GameConfig.player_inbox。测试用它绕开配置文件;传 false 的 overrideConfig 恢复读配置。
	static void SetConsumerConfigForTest(bool overrideConfig, bool consumerEnabled, uint32_t periodicFetchMinutes);
};
