#include "player/system/player_inbox_system.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <hiredis/hiredis.h>
#include <muduo/contrib/hiredis/Hiredis.h>
#include "muduo/base/Logging.h"

#include <node_config_manager.h>
#include "battle/system/player_battle.h"
#include "engine/core/type_define/player_guid_comp.h"
#include "grpc_client/grpc_call_tag.h"          // kGrpcStatusUnimplemented(与 grpc::StatusCode 同值)
#include "grpc_client/player_inbox/player_inbox_grpc_client.h"
#include "muduo_grpc/channel.h"                 // 选点读 muduo_grpc::Channel::GetState / ConnectivityState
#include "network/node_utils.h"                 // GetNodeInfo:Fetch 带上本节点 node_id(只用于对端日志)
#include "node/system/grpc_call_deadline.h"     // 在途超时从 PlayerInboxNodeService 的 gRPC deadline 派生
#include "node/system/node/muduo_grpc_channel_slot.h" // MuduoGrpcChannelPtr:节点实体上的 gRPC 通道组件
#include "player/comp/last_persisted_snapshot_comp.h"
#include "player/comp/player_inbox_runtime_comp.h"
#include "player/comp/player_ownership_comp.h"
#include "player/system/player_inbox_registry.h"
#include "player/system/player_inbox_rules.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_mutability.h"
#include "proto/common/base/common.pb.h"        // NodeInfo
#include "proto/common/base/node.pb.h"          // PlayerInboxNodeService
#include "proto/common/component/player_inbox_ledger_comp.pb.h"
#include "proto/player_inbox/player_inbox.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "thread_context/ecs_context.h"
#include "thread_context/node_context_manager.h"
#include "thread_context/redis_manager.h"
#include "utils/random/random.h"

namespace
{
	using player_inbox_rules::DeliveryClass;
	using player_inbox_rules::Durability;

	constexpr uint32_t kGrpcUnimplemented = kGrpcStatusUnimplemented; // 12,与 grpc::StatusCode::UNIMPLEMENTED 同值
	// 在途请求的兜底期限 = 该类节点的 gRPC deadline + 5s(上游比下游宽,grpc-client-deadline-failure-callback.md §4.2)。
	// 生成的客户端保证每次调用在 deadline 内以应答或失败之一收场,正常走不到这里;它只防"完成通知永远不来"。
	constexpr uint64_t kInflightSlackMs = 5000;

	// ── 可注入依赖(AGENTS §11.2 显式依赖;生产取默认值)────────────────────────

	// 进程内的退避、去抖、超时一律用单调时钟(AGENTS §11.3):系统时间被回拨时不能让检查长期不放行。
	uint64_t DefaultSteadyNowMs()
	{
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
										 std::chrono::steady_clock::now().time_since_epoch())
										 .count());
	}

	// 只在候选里挑**挂着 gRPC 通道且通道 READY** 的节点(照 id_segment_bootstrap.cpp 的 PickReadyDataServiceNode):
	// ConnectToGrpcNode 先 emplace MuduoGrpcChannelPtr 再 emplace NodeInfo,两者同在才是可用连接(第 3 批起没有 stub
	// 组件,grpc-muduo-transport.md §14);摘节点 / 关机阶段 0 先摘通道组件,正在拆的节点不会被挑中。
	// 通道不 READY 时发出去也只会换回一次 UNAVAILABLE,直接回"发不出去",由调用方退避。
	// tryToConnect=true 顺手把 IDLE 的通道(连接断开后回 IDLE;muduo 通道没有空闲超时)踢去重连。收件箱是全局服务,不比对 zone。
	entt::entity PickReadyPlayerInboxNode()
	{
		auto &registry = tlsNodeContextManager.GetRegistry(common::base::PlayerInboxNodeService);
		std::vector<entt::entity> ready;
		for (auto entity : registry.view<NodeInfo, MuduoGrpcChannelPtr>())
		{
			const auto &channel = registry.get<MuduoGrpcChannelPtr>(entity);
			if (channel && channel->GetState(/*tryToConnect=*/true) == muduo_grpc::ConnectivityState::kReady)
			{
				ready.push_back(entity);
			}
		}
		if (ready.empty())
		{
			return entt::null;
		}
		return ready[tlsRandom.Rand<std::size_t>(0, ready.size() - 1)];
	}

	bool DefaultSendFetch(const player_inbox::FetchPlayerInboxRequest &request)
	{
		const entt::entity node = PickReadyPlayerInboxNode();
		if (node == entt::null)
		{
			return false;
		}
		player_inbox::SendPlayerInboxFetchItems(tlsNodeContextManager.GetRegistry(common::base::PlayerInboxNodeService),
												node, request);
		return true;
	}

	bool DefaultSendAck(const player_inbox::AckPlayerInboxRequest &request)
	{
		const entt::entity node = PickReadyPlayerInboxNode();
		if (node == entt::null)
		{
			return false;
		}
		player_inbox::SendPlayerInboxAckItems(tlsNodeContextManager.GetRegistry(common::base::PlayerInboxNodeService),
											  node, request);
		return true;
	}

	PlayerInboxSystem::PersistFn gPersistFn = &PlayerLifecycleSystem::SavePlayerToRedis;
	PlayerInboxSystem::NowMsFn gNowMsFn = &DefaultSteadyNowMs;
	PlayerInboxSystem::SendFetchFn gSendFetchFn = &DefaultSendFetch;
	PlayerInboxSystem::SendAckFn gSendAckFn = &DefaultSendAck;

	struct ConsumerConfigOverride
	{
		bool active{false};
		bool consumerEnabled{false};
		uint32_t periodicFetchMinutes{0};
	};
	ConsumerConfigOverride gConfigOverride;

	// 关联号:本节点(loop 线程)单调递增,不按实体从头计(设计 §7.2)。已销毁实体的在途回包因此不可能与
	// 新实体的请求同号;0 保留给"没有在途"。
	thread_local uint64_t tlsCorrelationSeq = 0;
	// 注册表自检失败 = 本节点停用消费(PlayerInboxSystem::Initialize)。
	thread_local bool tlsRegistryBroken = false;

	uint64_t NextCorrelationId()
	{
		return ++tlsCorrelationSeq;
	}

	bool ConsumerEnabled()
	{
		if (tlsRegistryBroken)
		{
			return false;
		}
		if (gConfigOverride.active)
		{
			return gConfigOverride.consumerEnabled;
		}
		return tlsNodeConfigManager.GetGameConfig().player_inbox().consumer_enabled();
	}

	uint32_t PeriodicFetchMinutes()
	{
		if (gConfigOverride.active)
		{
			return gConfigOverride.periodicFetchMinutes;
		}
		return tlsNodeConfigManager.GetGameConfig().player_inbox().periodic_fetch_minutes();
	}

	uint32_t JitterPermille()
	{
		return tlsRandom.Rand<uint32_t>(0, player_inbox_rules::kJitterPermilleMax);
	}

	uint64_t InflightTimeoutMs()
	{
		return static_cast<uint64_t>(grpc_call_deadline::Get(common::base::PlayerInboxNodeService).count()) +
			   kInflightSlackMs;
	}

	// 收件箱只用 TipInfoMessage 表达错误(设计 §1.4);0 与 kSuccess 都是成功(serverbase.TipVerdict 同口径)。
	bool IsTipOk(uint32_t tipId)
	{
		return tipId == static_cast<uint32_t>(kCommon_errorOK) || tipId == static_cast<uint32_t>(kSuccess);
	}

	uint64_t GuidOf(entt::entity player)
	{
		const auto *guid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(player);
		return guid != nullptr ? guid->value : 0;
	}

	uint64_t OwnerEpochOf(entt::entity player)
	{
		const auto *ownerEpoch = tlsEcs.actorRegistry.try_get<PlayerOwnerEpochComp>(player);
		return ownerEpoch != nullptr ? ownerEpoch->epoch : 0;
	}

	// 异步回调里按 player_id 重新找实体:实体可能已销毁,槽位也可能已被别的玩家复用(Guid 不同)。
	entt::entity FindSamePlayer(uint64_t playerId)
	{
		if (playerId == 0)
		{
			return entt::null;
		}
		const entt::entity player = tlsEcs.GetPlayer(playerId);
		if (player == entt::null || !tlsEcs.actorRegistry.valid(player) || GuidOf(player) != playerId)
		{
			return entt::null;
		}
		return player;
	}

	PlayerInboxDomainState *FindDomainState(PlayerInboxRuntimeComp &runtime, uint32_t domain)
	{
		for (auto &state : runtime.domains)
		{
			if (state.domain == domain)
			{
				return &state;
			}
		}
		return nullptr;
	}

	// 回包 / PING 回调用:按 player_id 找回实体与该域状态,任何一步找不到都返回 nullptr(调用方丢弃)。
	PlayerInboxDomainState *FindLiveState(uint64_t playerId, uint32_t domain, entt::entity &player)
	{
		player = FindSamePlayer(playerId);
		if (player == entt::null)
		{
			return nullptr;
		}
		auto *runtime = tlsEcs.actorRegistry.try_get<PlayerInboxRuntimeComp>(player);
		return runtime != nullptr ? FindDomainState(*runtime, domain) : nullptr;
	}

	// Tick 收集到的待办只记实体句柄;处理前按句柄重新取组件(收集与处理之间没有其他回调插入,句柄不会过期,
	// 但仍走 valid + try_get,不赌这一点)。
	PlayerInboxDomainState *FindStateByEntity(entt::entity player, uint32_t domain)
	{
		if (!tlsEcs.actorRegistry.valid(player))
		{
			return nullptr;
		}
		auto *runtime = tlsEcs.actorRegistry.try_get<PlayerInboxRuntimeComp>(player);
		return runtime != nullptr ? FindDomainState(*runtime, domain) : nullptr;
	}

	// 按需状态组件的**唯一**延迟创建入口(docs/design/ecs-component-access-rules.md 规则 2、3):
	// 只由 OnPlayerLanded 在判定为新落地之后调用。其余路径一律 try_get,拿不到就丢弃。
	PlayerInboxRuntimeComp &StoreInboxRuntimeForWrite(entt::entity player)
	{
		// 此处延迟创建:实体第一次在本节点落地时才挂运行时组件("没有组件"本身就是新落地判据)。
		return tlsEcs.actorRegistry.get_or_emplace<PlayerInboxRuntimeComp>(player);
	}

	PlayerInboxDomainState &DomainStateForWrite(PlayerInboxRuntimeComp &runtime, uint32_t domain)
	{
		if (auto *existing = FindDomainState(runtime, domain); existing != nullptr)
		{
			return *existing;
		}
		auto &added = runtime.domains.emplace_back();
		added.domain = domain;
		return added;
	}

	const char *OutcomeVerb(uint32_t outcome)
	{
		switch (outcome)
		{
		case player_inbox::PLAYER_INBOX_OUTCOME_APPLIED:
			return "applied";
		case player_inbox::PLAYER_INBOX_OUTCOME_REJECTED:
			return "rejected";
		case player_inbox::PLAYER_INBOX_OUTCOME_SKIPPED:
			return "skipped";
		case player_inbox::PLAYER_INBOX_OUTCOME_UNKNOWN:
			return "unknown";
		default:
			return "?";
		}
	}

	// ── 退避与停用 ──────────────────────────────────────────────────────────

	// 记下延后原因并在 delayMs 之后续做。同一原因连续出现只在第一次打 INFO,免得收件箱不可用时
	// 每个在线玩家每分钟各刷一行(DEBUG 仍逐次记)。
	void ScheduleRetry(PlayerInboxDomainState &state, uint64_t playerId, PlayerInboxDeferReason reason,
					   uint64_t delayMs, uint64_t nowMs)
	{
		state.nextCheckMs = nowMs + delayMs;
		if (state.deferReason != reason)
		{
			LOG_INFO << "[PlayerInbox] metric=inbox_deferred reason=" << PlayerInboxDeferReasonName(reason)
					 << " player_id=" << playerId << " domain=" << state.domain
					 << " phase=" << PlayerInboxPhaseName(state.phase) << " retry_in_ms=" << delayMs;
		}
		else
		{
			LOG_DEBUG << "[PlayerInbox] deferred again reason=" << PlayerInboxDeferReasonName(reason)
					  << " player_id=" << playerId << " domain=" << state.domain << " streak=" << state.failStreak
					  << " retry_in_ms=" << delayMs;
		}
		state.deferReason = reason;
	}

	// 按 1s → 60s 加抖动退避(设计 §7.5)。消费关闭不安排重试:配置在进程内不会翻转。
	void Defer(PlayerInboxDomainState &state, uint64_t playerId, PlayerInboxDeferReason reason, uint64_t nowMs)
	{
		if (reason == PlayerInboxDeferReason::kDisabled)
		{
			state.nextCheckMs = 0;
			state.deferReason = reason;
			return;
		}
		if (state.failStreak < std::numeric_limits<uint32_t>::max())
		{
			++state.failStreak;
		}
		ScheduleRetry(state, playerId, reason, player_inbox_rules::BackoffMs(state.failStreak, JitterPermille()), nowMs);
	}

	// 停用这名玩家的这个域:本实体不再 Fetch、不再应用,直到实体下次在本节点新建(设计 §7.6、§7.7)。
	// ackQueue 与阶段由调用方决定(有的路径还要确认已定的前缀,有的路径什么都不确认)。
	void Halt(PlayerInboxDomainState &state, uint64_t playerId, PlayerInboxHaltReason reason, const std::string &detail)
	{
		state.halted = true;
		state.haltReason = reason;
		state.pendingItems.clear();
		state.nextCheckMs = 0;
		state.recheckRequested = false;
		state.hasMore = false;
		LOG_ERROR << "[PlayerInbox] metric=inbox_halted reason=" << PlayerInboxHaltReasonName(reason)
				  << " player_id=" << playerId << " domain=" << state.domain << " detail=" << detail
				  << "; this domain stays disabled for the player until the entity is recreated on this node";
	}

	// 一轮结束:回到 Idle。has_more 立即拉下一批;期间来过新触发则去抖后再拉一次。
	void FinishRound(PlayerInboxDomainState &state, uint64_t nowMs)
	{
		state.phase = PlayerInboxPhase::kIdle;
		state.correlationId = 0;
		state.inflightSinceMs = 0;
		state.durableMismatchReported = false;
		if (state.halted)
		{
			state.nextCheckMs = 0;
		}
		else if (state.hasMore)
		{
			state.nextCheckMs = nowMs;
		}
		else if (state.recheckRequested)
		{
			state.nextCheckMs = std::max(nowMs, state.lastFetchMs + player_inbox_rules::kMinCheckGapMs);
		}
		else
		{
			state.nextCheckMs = 0;
		}
		state.hasMore = false;
		state.recheckRequested = false;
	}

	// ── 前置闸(设计 §7.5)──────────────────────────────────────────────────

	// 返回 kNone = 放行。拉取前判一次、应用前再判一次;确认不经过它。
	PlayerInboxDeferReason CheckApplyGate(entt::entity player, uint32_t domain)
	{
		if (!ConsumerEnabled())
		{
			return PlayerInboxDeferReason::kDisabled;
		}
		switch (player_mutability::ClassifyPlayerMutability(player))
		{
		case player_mutability::PlayerMutability::kMutable:
			break;
		case player_mutability::PlayerMutability::kFrozen:
			return PlayerInboxDeferReason::kFrozen;
		case player_mutability::PlayerMutability::kTravelHandoff:
			return PlayerInboxDeferReason::kHandoff;
		case player_mutability::PlayerMutability::kExiting:
			return PlayerInboxDeferReason::kExiting;
		case player_mutability::PlayerMutability::kOwnerEpochUnknown:
		case player_mutability::PlayerMutability::kCount:
		default:
			return PlayerInboxDeferReason::kEpochZero;
		}
		// 持久账本只由加载创建;缺了等同于账本无效(fail-closed,绝不补建一本空账本:空账本会把已执行过的
		// 欠款判成"没见过"再挂一次)。
		if (tlsEcs.actorRegistry.any_of<PlayerInboxLedgerInvalidComp>(player) ||
			!tlsEcs.actorRegistry.all_of<PlayerInboxLedgerComp>(player))
		{
			return PlayerInboxDeferReason::kLedgerInvalid;
		}
		// 照 player_rollback_handler 对 GM 写操作的战斗闸:战斗快照已经发出去,此刻改货币状态会与结算打架。
		if (player_inbox_registry::DomainRule(domain).requiresOutOfBattle && PlayerBattleSystem::IsInBattle(player))
		{
			return PlayerInboxDeferReason::kInBattle;
		}
		return PlayerInboxDeferReason::kNone;
	}

	// ── 落盘与确认(设计 §7.7)──────────────────────────────────────────────

	void OnPersistProbe(uint64_t playerId, uint32_t domain);

	// 请求一次存盘(500ms 节流),压了写就在同一条连接上紧跟一条 PING 走快路径。
	void RequestPersist(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		if (state.lastPersistRequestMs != 0 && nowMs - state.lastPersistRequestMs < player_inbox_rules::kPersistThrottleMs)
		{
			return;
		}
		// 不可改的实体不压存盘:冻结 / 交接在途时本节点无权再写盘,退出在途时退出存盘已经在飞
		// (它 marshal 的那一份本来就含账本,落地即 durable)。跳过不影响正确性:durable 仍只看快照。
		if (!player_mutability::IsPlayerMutable(player))
		{
			return;
		}
		state.lastPersistRequestMs = nowMs;
		if (!gPersistFn(player))
		{
			// false 可能是"盘上已是这一份",也可能是被拒存;一律交给 durable 判定按快照字节裁决。
			return;
		}
		// 快路径(照 player_battle.cpp 的 RequestSettlementPersist):玩家存盘客户端与 tlsRedis.GetZoneRedis()
		// 是同一个 hiredis 连接,回复按发送顺序处理;存盘成功回调同步更新 PlayerLastPersistedSnapshotComp。
		// 所以 PING 回来时,刚才那笔存盘若已直接发出,快照就已是新的。没赶上的由 tick 每秒复查兜住。
		auto &redis = tlsRedis.GetZoneRedis();
		if (!redis || !redis->connected())
		{
			return;
		}
		const uint32_t domain = state.domain;
		redis->command([playerId, domain](hiredis::Hiredis *, redisReply *) { OnPersistProbe(playerId, domain); },
					   "PING");
	}

	// 本批每一条结局是否都已出现在最后一次落盘快照里(规则见 player_inbox_rules::ClassifyDurability)。
	bool AllDecisionsDurable(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state)
	{
		const auto *snapshotComp = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
		if (snapshotComp == nullptr || !snapshotComp->HasSnapshot())
		{
			return false;
		}
		const auto &persisted = snapshotComp->snapshot->player_database_data().inbox_ledger();
		for (const auto &decision : state.ackQueue)
		{
			const auto durability = player_inbox_rules::ClassifyDurability(persisted, state.domain, decision);
			if (durability == Durability::kDurable)
			{
				continue;
			}
			if (durability != Durability::kNotYet && !state.durableMismatchReported)
			{
				// 盘上与内存不一致:结局固定被破坏,只可能是缺陷。不敢报 durable、不确认,继续等并告警
				// (口径同资产通道 IsAssetOpDurable)。每批只报一次。
				state.durableMismatchReported = true;
				LOG_ERROR << "[PlayerInbox] Persisted ledger disagrees with in-memory decision; not acknowledging player_id="
						  << playerId << " domain=" << state.domain << " seq=" << decision.seq()
						  << " outcome=" << OutcomeVerb(decision.outcome())
						  << " verdict=" << player_inbox_rules::DurabilityName(durability);
			}
			return false;
		}
		return true;
	}

	void SendAck(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		const uint64_t epoch = OwnerEpochOf(player);
		if (epoch == 0)
		{
			// 拉取前已过 epoch 闸,epoch 只增不减:走到这里只可能是组件被摘(缺陷)。发 0 会被判 EPOCH_REQUIRED
			// 并停用,不如保留 ackQueue 退避再看;实体若随后销毁,下次落地从账本补确认。
			if (state.ackFailStreak < std::numeric_limits<uint32_t>::max())
			{
				++state.ackFailStreak;
			}
			state.nextAckMs = nowMs + player_inbox_rules::BackoffMs(state.ackFailStreak, JitterPermille());
			LOG_ERROR << "[PlayerInbox] Ack postponed: owner_epoch missing on a player that already fetched player_id="
					  << playerId << " domain=" << state.domain;
			return;
		}

		player_inbox::AckPlayerInboxRequest request;
		request.set_player_id(playerId);
		request.set_domain(static_cast<player_inbox::PlayerInboxDomain>(state.domain));
		request.set_owner_epoch(epoch);
		const uint64_t correlationId = NextCorrelationId();
		request.set_correlation_id(correlationId);
		for (const auto &decision : state.ackQueue)
		{
			auto *report = request.add_decisions();
			report->set_seq(decision.seq());
			report->set_outcome(static_cast<player_inbox::PlayerInboxOutcome>(decision.outcome()));
			report->set_reason(decision.reason());
			report->set_request_id(decision.request_id()); // 服务端再核对一次(IB-9)
		}

		if (!gSendAckFn(request))
		{
			if (state.ackFailStreak < std::numeric_limits<uint32_t>::max())
			{
				++state.ackFailStreak;
			}
			state.nextAckMs = nowMs + player_inbox_rules::BackoffMs(state.ackFailStreak, JitterPermille());
			LOG_DEBUG << "[PlayerInbox] Ack not sent: no READY PlayerInboxNodeService node player_id=" << playerId
					  << " domain=" << state.domain << " streak=" << state.ackFailStreak;
			return;
		}
		state.phase = PlayerInboxPhase::kAcking;
		state.correlationId = correlationId;
		state.inflightSinceMs = nowMs;
	}

	// kAwaitDurable 的推进:全部落盘就确认;没落盘就(每 5 秒)补请求一次存盘。确认不受前置闸限制。
	void TryAck(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		if (state.phase != PlayerInboxPhase::kAwaitDurable)
		{
			return;
		}
		if (state.ackQueue.empty())
		{
			FinishRound(state, nowMs);
			return;
		}
		if (!AllDecisionsDurable(player, playerId, state))
		{
			if (state.lastPersistRequestMs == 0 || nowMs - state.lastPersistRequestMs >= player_inbox_rules::kResaveIntervalMs)
			{
				RequestPersist(player, playerId, state, nowMs);
			}
			return;
		}
		if (nowMs < state.nextAckMs)
		{
			return;
		}
		SendAck(player, playerId, state, nowMs);
	}

	// 本批条目处理完(或在某一条停下)之后:有新记账就请求存盘;有待确认的进 kAwaitDurable,否则本轮结束。
	void AfterBatchDecided(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, bool recorded,
						   uint64_t nowMs)
	{
		if (recorded)
		{
			RequestPersist(player, playerId, state, nowMs);
		}
		if (state.ackQueue.empty())
		{
			FinishRound(state, nowMs);
			return;
		}
		state.phase = PlayerInboxPhase::kAwaitDurable;
		state.nextAckMs = nowMs;
		state.ackFailStreak = 0;
		state.durableMismatchReported = false;
		TryAck(player, playerId, state, nowMs);
	}

	// ── 应用(设计 §7.6 第 3 步)────────────────────────────────────────────

	// 按 seq 升序逐条处理 pendingItems。在同一个 loop 帧里写效果、推进账本、压入最近结局环;
	// 在某一条停下时,已定的前缀照常进入确认流程(MISMATCH 除外:那时什么都不确认)。
	void ApplyPending(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		if (state.halted)
		{
			state.pendingItems.clear();
			AfterBatchDecided(player, playerId, state, false, nowMs);
			return;
		}
		if (const auto reason = CheckApplyGate(player, state.domain); reason != PlayerInboxDeferReason::kNone)
		{
			// 条目留在本地(kApplying),闸过了再续做:不重新 Fetch,也就不白涨服务端的 delivery_count。
			Defer(state, playerId, reason, nowMs);
			return;
		}
		auto &ledger = tlsEcs.actorRegistry.get<PlayerInboxLedgerComp>(player); // 闸已确认组件在

		bool recorded = false;
		for (std::size_t consumed = 0; consumed < state.pendingItems.size(); ++consumed)
		{
			const auto &item = state.pendingItems[consumed];
			const uint64_t seq = item.seq();
			const uint64_t requestId = item.request_id();
			const uint32_t msgType = static_cast<uint32_t>(item.msg_type());

			// 1. 本二进制不认识的类型,或类型不属于本域:版本错位(新类型先于 scene 升级被启用)。
			//    在这一条停下、不记账、不确认这一条,停用该域 —— 绝不能当成终态的 REJECTED。
			const auto executor = player_inbox_registry::FindExecutor(msgType, state.domain);
			if (executor == nullptr)
			{
				LOG_ERROR << "[PlayerInbox] metric=inbox_unknown_type player_id=" << playerId << " domain=" << state.domain
						  << " seq=" << seq << " msg_type=" << msgType << " request_id=" << requestId;
				Halt(state, playerId, PlayerInboxHaltReason::kUnknownType, "msg_type=" + std::to_string(msgType));
				AfterBatchDecided(player, playerId, state, recorded, nowMs);
				return;
			}

			// 2. 重复投递:按 (seq, request_id, msg_type) 核对后回放(IB-9)。
			PlayerInboxDecision replayed;
			const auto deliveryClass = player_inbox_rules::ClassifyDelivery(
				player_inbox_rules::FindCursor(ledger, state.domain), seq, requestId, msgType, &replayed);
			if (deliveryClass == DeliveryClass::kReplay)
			{
				state.ackQueue.push_back(replayed);
				continue;
			}
			if (deliveryClass == DeliveryClass::kUnknown)
			{
				// 水位已过这一条、原结局已不在环里:只可能来自之前会话已落盘的水位,报 UNKNOWN(设计 §7.7)。
				PlayerInboxDecision unknown;
				unknown.set_seq(seq);
				unknown.set_outcome(static_cast<uint32_t>(player_inbox::PLAYER_INBOX_OUTCOME_UNKNOWN));
				unknown.set_reason(0);
				unknown.set_request_id(requestId);
				unknown.set_msg_type(msgType);
				state.ackQueue.push_back(unknown);
				LOG_WARN << "[PlayerInbox] Redelivered entry is below the ledger watermark but no longer in the recent ring;"
						 << " acknowledging as UNKNOWN player_id=" << playerId << " domain=" << state.domain
						 << " seq=" << seq << " request_id=" << requestId;
				continue;
			}
			if (deliveryClass == DeliveryClass::kMismatch)
			{
				LOG_ERROR << "[PlayerInbox] metric=inbox_replay_mismatch player_id=" << playerId << " domain=" << state.domain
						  << " seq=" << seq << " delivered_request_id=" << requestId << " delivered_msg_type=" << msgType
						  << " ledger_request_id=" << replayed.request_id() << " ledger_msg_type=" << replayed.msg_type();
				// 什么都不确认(设计 §7.6):本批已记账的前缀照常随存盘落地,由之后的会话按账本补确认。
				state.ackQueue.clear();
				Halt(state, playerId, PlayerInboxHaltReason::kReplayMismatch, "seq=" + std::to_string(seq));
				if (recorded)
				{
					RequestPersist(player, playerId, state, nowMs);
				}
				FinishRound(state, nowMs);
				return;
			}

			// 3. 新条目:作废指令不解码载荷,其余交给执行器。
			PlayerInboxDecision decision;
			decision.set_seq(seq);
			decision.set_request_id(requestId);
			decision.set_msg_type(msgType);
			const auto directive = item.directive();
			if (directive == player_inbox::PLAYER_INBOX_DIRECTIVE_VOID)
			{
				decision.set_outcome(static_cast<uint32_t>(player_inbox::PLAYER_INBOX_OUTCOME_SKIPPED));
				decision.set_reason(static_cast<uint32_t>(item.void_reason()));
			}
			else if (directive != player_inbox::PLAYER_INBOX_DIRECTIVE_NONE)
			{
				// 不认识的指令同样是版本错位:停在这一条,不猜。
				LOG_ERROR << "[PlayerInbox] metric=inbox_unknown_type player_id=" << playerId << " domain=" << state.domain
						  << " seq=" << seq << " directive=" << static_cast<int>(directive) << " request_id=" << requestId;
				Halt(state, playerId, PlayerInboxHaltReason::kUnknownType,
					 "directive=" + std::to_string(static_cast<int>(directive)));
				AfterBatchDecided(player, playerId, state, recorded, nowMs);
				return;
			}
			else
			{
				const PlayerInboxExecResult result = executor(player, item.payload());
				if (result.kind == PlayerInboxExecKind::kApplied)
				{
					decision.set_outcome(static_cast<uint32_t>(player_inbox::PLAYER_INBOX_OUTCOME_APPLIED));
					decision.set_reason(0);
				}
				else if (result.kind == PlayerInboxExecKind::kRejected)
				{
					decision.set_outcome(static_cast<uint32_t>(player_inbox::PLAYER_INBOX_OUTCOME_REJECTED));
					decision.set_reason(result.reason);
				}
				else
				{
					// DEFER:严格顺序,停在这一条;已处理的前缀从本地缓存里摘掉,剩下的按退避续做,不重新 Fetch。
					LOG_WARN << "[PlayerInbox] Executor deferred player_id=" << playerId << " domain=" << state.domain
							 << " seq=" << seq << " type=" << player_inbox_registry::TypeName(msgType)
							 << " reason=" << result.reason;
					state.pendingItems.erase(state.pendingItems.begin(),
											 state.pendingItems.begin() + static_cast<std::ptrdiff_t>(consumed));
					if (recorded)
					{
						RequestPersist(player, playerId, state, nowMs);
					}
					Defer(state, playerId, PlayerInboxDeferReason::kExecutorDefer, nowMs);
					return;
				}
			}

			// 记账:与效果同一帧,推进水位并压入环(超过 32 条挤掉最旧的)。
			if (!player_inbox_rules::RecordDecision(player_inbox_rules::MutableCursor(ledger, state.domain), decision))
			{
				// 分类刚说"未见",记账却拒绝:前置被破坏,只可能是缺陷。效果已经写了,结局却没记 ——
				// 停用该域、不确认这一条;已定的前缀照常确认。
				LOG_ERROR << "[PlayerInbox] Ledger precondition violated after the effect was applied player_id="
						  << playerId << " domain=" << state.domain << " seq=" << seq
						  << " outcome=" << OutcomeVerb(decision.outcome());
				Halt(state, playerId, PlayerInboxHaltReason::kLedgerWrite, "seq=" + std::to_string(seq));
				AfterBatchDecided(player, playerId, state, recorded, nowMs);
				return;
			}
			recorded = true;
			state.ackQueue.push_back(decision);
			// 可检索的审计行(设计 §7.6):库按备份恢复后,运维对照它与工单人工核对。
			LOG_INFO << "[PlayerInbox] " << OutcomeVerb(decision.outcome())
					 << " type=" << player_inbox_registry::TypeName(msgType) << " request_id=" << requestId
					 << " seq=" << seq << " player_id=" << playerId << " domain=" << state.domain
					 << " reason=" << decision.reason();
		}

		state.pendingItems.clear();
		state.failStreak = 0;
		state.deferReason = PlayerInboxDeferReason::kNone;
		AfterBatchDecided(player, playerId, state, recorded, nowMs);
	}

	// ── 拉取 ────────────────────────────────────────────────────────────────

	void StartFetch(entt::entity player, uint64_t playerId, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		if (const auto reason = CheckApplyGate(player, state.domain); reason != PlayerInboxDeferReason::kNone)
		{
			Defer(state, playerId, reason, nowMs);
			return;
		}
		const auto &ledger = tlsEcs.actorRegistry.get<PlayerInboxLedgerComp>(player); // 闸已确认组件在
		const auto *cursor = player_inbox_rules::FindCursor(ledger, state.domain);
		const uint64_t epoch = OwnerEpochOf(player); // 闸已确认非 0

		player_inbox::FetchPlayerInboxRequest request;
		request.set_player_id(playerId);
		request.set_domain(static_cast<player_inbox::PlayerInboxDomain>(state.domain));
		request.set_owner_epoch(epoch);
		const uint64_t correlationId = NextCorrelationId();
		request.set_correlation_id(correlationId);
		request.set_max_items(player_inbox_rules::kFetchBatchMax);
		// 账本水位:服务端据此检测全局库被恢复(CursorAhead)并抬号(设计 §7.8)。
		request.set_consumer_decided_through(cursor != nullptr ? cursor->decided_through_seq() : 0);
		request.set_scene_node_id(GetNodeInfo().node_id());

		if (!gSendFetchFn(request))
		{
			Defer(state, playerId, PlayerInboxDeferReason::kNoNode, nowMs);
			return;
		}
		state.phase = PlayerInboxPhase::kFetching;
		state.correlationId = correlationId;
		state.inflightSinceMs = nowMs;
		state.sentEpoch = epoch;
		state.lastFetchMs = nowMs;
		state.recheckRequested = false;
	}

	// 回包形状校验:超过一批上限、seq 为 0 或不严格递增都是协议异常。返回空串 = 合法。
	std::string DescribeMalformedBatch(const player_inbox::FetchPlayerInboxResponse &response)
	{
		if (response.items_size() > static_cast<int>(player_inbox_rules::kFetchBatchMax))
		{
			return "items=" + std::to_string(response.items_size()) + " exceeds batch max " +
				   std::to_string(player_inbox_rules::kFetchBatchMax);
		}
		uint64_t previousSeq = 0;
		for (const auto &item : response.items())
		{
			if (item.seq() == 0)
			{
				return "seq 0 in batch";
			}
			if (item.seq() <= previousSeq)
			{
				return "seq " + std::to_string(item.seq()) + " not strictly ascending after " + std::to_string(previousSeq);
			}
			previousSeq = item.seq();
		}
		return {};
	}

	void OnPersistProbe(uint64_t playerId, uint32_t domain)
	{
		entt::entity player = entt::null;
		auto *state = FindLiveState(playerId, domain, player);
		if (state == nullptr || state->phase != PlayerInboxPhase::kAwaitDurable)
		{
			return;
		}
		TryAck(player, playerId, *state, gNowMsFn());
	}

	// 在途请求超过期限仍无完成通知:按传输失败收场(结果未知)。迟到的回包对不上关联号,会被丢弃。
	void HandleInflightTimeout(entt::entity player, PlayerInboxDomainState &state, uint64_t nowMs)
	{
		const uint64_t playerId = GuidOf(player);
		LOG_WARN << "[PlayerInbox] Request got no completion in time; treating as transport failure player_id=" << playerId
				 << " domain=" << state.domain << " phase=" << PlayerInboxPhaseName(state.phase)
				 << " corr=" << state.correlationId << " inflight_ms=" << (nowMs - state.inflightSinceMs);
		state.correlationId = 0;
		state.inflightSinceMs = 0;
		if (state.phase == PlayerInboxPhase::kFetching)
		{
			state.phase = PlayerInboxPhase::kIdle;
			Defer(state, playerId, PlayerInboxDeferReason::kTransport, nowMs);
			return;
		}
		// kAcking:保留 ackQueue,退避重发。
		state.phase = PlayerInboxPhase::kAwaitDurable;
		if (state.ackFailStreak < std::numeric_limits<uint32_t>::max())
		{
			++state.ackFailStreak;
		}
		state.nextAckMs = nowMs + player_inbox_rules::BackoffMs(state.ackFailStreak, JitterPermille());
	}

	struct DueWork
	{
		entt::entity player{entt::null};
		uint32_t domain{0};
	};
} // namespace

// ── 启动 ────────────────────────────────────────────────────────────────────

void PlayerInboxSystem::Initialize()
{
	const std::string selfCheck = player_inbox_registry::SelfCheck();
	tlsRegistryBroken = !selfCheck.empty();
	if (tlsRegistryBroken)
	{
		// 不阻断启动(IB-7):收件箱停用只是延迟处置,条目留在服务端 PENDING。
		LOG_ERROR << "[PlayerInbox] Registry self-check failed; inbox consumption disabled on this node: " << selfCheck;
	}
	LOG_INFO << "[PlayerInbox] consumer_enabled=" << ConsumerEnabled()
			 << " periodic_fetch_minutes=" << PeriodicFetchMinutes()
			 << " consumer_domains=" << player_inbox_registry::ConsumerDomains().size();
}

// ── 触发入口 ────────────────────────────────────────────────────────────────

void PlayerInboxSystem::OnPlayerLanded(entt::entity player)
{
	if (!tlsEcs.actorRegistry.valid(player))
	{
		return;
	}
	const uint64_t playerId = GuidOf(player);
	if (playerId == 0)
	{
		return;
	}
	// 消费关闭:不建运行时组件、不发 Fetch;账本照常随存档往返(回滚下限,设计 §3.3、§11.7)。
	if (!ConsumerEnabled())
	{
		return;
	}
	const uint64_t epoch = OwnerEpochOf(player);
	const auto *existing = tlsEcs.actorRegistry.try_get<PlayerInboxRuntimeComp>(player);
	if (!player_inbox_rules::IsNewLanding(existing != nullptr, existing != nullptr ? existing->landedEpoch : 0, epoch))
	{
		// 同节点换图、gate 补发的重复路由:不算落地。
		LOG_DEBUG << "[PlayerInbox] landing skipped (same owner_epoch) player_id=" << playerId << " epoch=" << epoch;
		return;
	}

	auto &runtime = StoreInboxRuntimeForWrite(player);
	runtime.landedEpoch = epoch;
	const uint64_t nowMs = gNowMsFn();
	const uint32_t periodicMinutes = PeriodicFetchMinutes();
	for (const uint32_t domain : player_inbox_registry::ConsumerDomains())
	{
		auto &state = DomainStateForWrite(runtime, domain);
		if (state.halted)
		{
			continue;
		}
		state.nextPeriodicMs =
			periodicMinutes > 0 ? nowMs + player_inbox_rules::PeriodicDelayMs(periodicMinutes, JitterPermille()) : 0;
		switch (state.phase)
		{
		case PlayerInboxPhase::kApplying:
			// 归属变过:本地缓存的条目是旧归属下拉到的,丢掉重拉(它们在服务端仍是 PENDING,不会丢)。
			state.pendingItems.clear();
			state.phase = PlayerInboxPhase::kIdle;
			[[fallthrough]];
		case PlayerInboxPhase::kIdle:
			// 新落地:无条件拉一次,不去抖(设计 §7.3 第 1 条)。
			state.failStreak = 0;
			state.deferReason = PlayerInboxDeferReason::kNone;
			state.nextCheckMs = nowMs;
			break;
		default:
			// 单飞:在途的那一轮结束后再拉(设计 §7.4)。
			state.recheckRequested = true;
			break;
		}
	}
	LOG_DEBUG << "[PlayerInbox] landing scheduled a check player_id=" << playerId << " epoch=" << epoch;
}

void PlayerInboxSystem::OnBattleFreezeCleared(entt::entity player)
{
	if (!tlsEcs.actorRegistry.valid(player))
	{
		return;
	}
	auto *runtime = tlsEcs.actorRegistry.try_get<PlayerInboxRuntimeComp>(player);
	if (runtime == nullptr)
	{
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	for (auto &state : runtime->domains)
	{
		if (state.halted || state.deferReason != PlayerInboxDeferReason::kInBattle)
		{
			continue;
		}
		if (state.phase == PlayerInboxPhase::kIdle || state.phase == PlayerInboxPhase::kApplying)
		{
			// 立即重查:不合并、不去抖(协调项 C1)。不加这一行,战斗结束后最多要再等 60 秒的退避。
			state.failStreak = 0;
			state.nextCheckMs = nowMs;
		}
	}
}

void PlayerInboxSystem::OnNotifyEvent(uint64_t playerId, uint32_t domain)
{
	const entt::entity player = FindSamePlayer(playerId);
	if (player == entt::null)
	{
		// 提醒是"至多一次"的:玩家不在本节点就丢,下次落地会拉(设计 §6.3)。
		LOG_DEBUG << "[PlayerInbox] notify dropped: player not on this node player_id=" << playerId;
		return;
	}
	auto *runtime = tlsEcs.actorRegistry.try_get<PlayerInboxRuntimeComp>(player);
	if (runtime == nullptr)
	{
		LOG_DEBUG << "[PlayerInbox] notify dropped: player has not landed yet (the landing fetch covers it) player_id="
				  << playerId;
		return;
	}
	auto *state = FindDomainState(*runtime, domain);
	if (state == nullptr)
	{
		LOG_WARN << "[PlayerInbox] notify for a domain this binary does not consume dropped player_id=" << playerId
				 << " domain=" << domain;
		return;
	}
	if (state->halted)
	{
		return;
	}
	if (state->phase != PlayerInboxPhase::kIdle)
	{
		state->recheckRequested = true;
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	const uint64_t debounced = std::max(nowMs, state->lastFetchMs + player_inbox_rules::kMinCheckGapMs);
	if (state->nextCheckMs == 0 || state->nextCheckMs > debounced)
	{
		state->nextCheckMs = debounced;
	}
}

void PlayerInboxSystem::Tick()
{
	const uint64_t nowMs = gNowMsFn();
	const uint32_t periodicMinutes = PeriodicFetchMinutes();
	const uint64_t inflightTimeoutMs = InflightTimeoutMs();

	// 先只读遍历收集到期项,再逐项处理:处理里会调执行器、请求存盘,不在遍历中做这些。
	// 本函数只用 view / try_get,不创建任何组件(AGENTS §7.5)。
	std::vector<DueWork> timeouts;
	std::vector<DueWork> acks;
	std::vector<DueWork> checks;
	for (auto &&[entity, runtime] : tlsEcs.actorRegistry.view<PlayerInboxRuntimeComp>().each())
	{
		for (auto &state : runtime.domains)
		{
			// 在线有界兜底:计时只记在运行时,不进存档(设计 §7.3 第 3 条)。
			if (periodicMinutes > 0 && !state.halted && state.nextPeriodicMs != 0 && nowMs >= state.nextPeriodicMs)
			{
				state.nextPeriodicMs = nowMs + player_inbox_rules::PeriodicDelayMs(periodicMinutes, JitterPermille());
				if (state.phase == PlayerInboxPhase::kIdle)
				{
					if (state.nextCheckMs == 0)
					{
						state.nextCheckMs = nowMs;
					}
				}
				else
				{
					state.recheckRequested = true;
				}
			}

			switch (state.phase)
			{
			case PlayerInboxPhase::kFetching:
			case PlayerInboxPhase::kAcking:
				if (state.inflightSinceMs != 0 && nowMs - state.inflightSinceMs > inflightTimeoutMs)
				{
					timeouts.push_back(DueWork{entity, state.domain});
				}
				break;
			case PlayerInboxPhase::kAwaitDurable:
				acks.push_back(DueWork{entity, state.domain});
				break;
			case PlayerInboxPhase::kIdle:
			case PlayerInboxPhase::kApplying:
				if (!state.halted && state.nextCheckMs != 0 && nowMs >= state.nextCheckMs)
				{
					checks.push_back(DueWork{entity, state.domain});
				}
				break;
			case PlayerInboxPhase::kCount:
			default:
				break;
			}
		}
	}

	for (const auto &work : timeouts)
	{
		auto *state = FindStateByEntity(work.player, work.domain);
		if (state != nullptr &&
			(state->phase == PlayerInboxPhase::kFetching || state->phase == PlayerInboxPhase::kAcking))
		{
			HandleInflightTimeout(work.player, *state, nowMs);
		}
	}

	// 等落盘与确认:每秒复查一次快照(纯内存比较),每 5 秒补请求一次存盘。不受前置闸与检查预算限制。
	for (const auto &work : acks)
	{
		auto *state = FindStateByEntity(work.player, work.domain);
		if (state != nullptr && state->phase == PlayerInboxPhase::kAwaitDurable)
		{
			TryAck(work.player, GuidOf(work.player), *state, nowMs);
		}
	}

	// 到期的检查:单个 tick 最多发起 kTickCheckBudget 次拉取,其余保持到期、下一 tick 再做(设计 §7.4)。
	uint32_t budget = player_inbox_rules::kTickCheckBudget;
	for (const auto &work : checks)
	{
		auto *state = FindStateByEntity(work.player, work.domain);
		if (state == nullptr || state->halted || state->nextCheckMs == 0 || nowMs < state->nextCheckMs)
		{
			continue;
		}
		const uint64_t playerId = GuidOf(work.player);
		if (playerId == 0)
		{
			continue;
		}
		if (state->phase == PlayerInboxPhase::kIdle)
		{
			if (budget == 0)
			{
				continue;
			}
			--budget;
			state->nextCheckMs = 0;
			StartFetch(work.player, playerId, *state, nowMs);
		}
		else if (state->phase == PlayerInboxPhase::kApplying)
		{
			state->nextCheckMs = 0;
			ApplyPending(work.player, playerId, *state, nowMs);
		}
	}
}

// ── gRPC 回包 ───────────────────────────────────────────────────────────────

void PlayerInboxSystem::OnFetchReply(const player_inbox::FetchPlayerInboxResponse &response)
{
	const uint64_t playerId = response.player_id();
	const uint32_t domain = static_cast<uint32_t>(response.domain());
	entt::entity player = entt::null;
	auto *state = FindLiveState(playerId, domain, player);
	// 关联校验(设计 §7.6 第 1 步、§7.11):按回显的 player_id 找实体,domain 与 correlation_id 必须对上在途请求。
	if (state == nullptr || state->phase != PlayerInboxPhase::kFetching || response.correlation_id() == 0 ||
		state->correlationId != response.correlation_id())
	{
		LOG_DEBUG << "[PlayerInbox] Fetch reply discarded (entity gone, recreated or request superseded) player_id="
				  << playerId << " domain=" << domain << " corr=" << response.correlation_id();
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	state->phase = PlayerInboxPhase::kIdle;
	state->correlationId = 0;
	state->inflightSinceMs = 0;

	const auto fence = response.fence();
	if (fence == player_inbox::PLAYER_INBOX_FENCE_STALE)
	{
		LOG_ERROR << "[PlayerInbox] metric=inbox_stale_owner op=fetch player_id=" << playerId << " domain=" << domain
				  << " sent_epoch=" << state->sentEpoch;
		ScheduleRetry(*state, playerId, PlayerInboxDeferReason::kStaleOwner, player_inbox_rules::kStaleOwnerBackoffMs,
					  nowMs);
		return;
	}
	if (fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_REQUIRED || fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_AHEAD)
	{
		// 缺陷或伪造(IB-8):停用,不重试。
		Halt(*state, playerId,
			 fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_REQUIRED ? PlayerInboxHaltReason::kEpochRequired
																	   : PlayerInboxHaltReason::kEpochAhead,
			 "op=fetch sent_epoch=" + std::to_string(state->sentEpoch));
		FinishRound(*state, nowMs);
		return;
	}
	if (const uint32_t tip = response.error_message().id(); !IsTipOk(tip))
	{
		// 服务端故障(例如 kPlayerInboxStoreUnavailable):退避,不计协议异常。
		if (state->deferReason != PlayerInboxDeferReason::kServerError)
		{
			LOG_WARN << "[PlayerInbox] Fetch answered with tip=" << tip << " player_id=" << playerId
					 << " domain=" << domain;
		}
		Defer(*state, playerId, PlayerInboxDeferReason::kServerError, nowMs);
		return;
	}
	if (fence == player_inbox::PLAYER_INBOX_FENCE_NO_STREAM || response.items_size() == 0)
	{
		// 本轮结束。空批不信 has_more,免得对着一个空回包反复空转。
		state->failStreak = 0;
		state->deferReason = PlayerInboxDeferReason::kNone;
		state->hasMore = false;
		FinishRound(*state, nowMs);
		return;
	}
	if (fence != player_inbox::PLAYER_INBOX_FENCE_FIRST && fence != player_inbox::PLAYER_INBOX_FENCE_MATCH &&
		fence != player_inbox::PLAYER_INBOX_FENCE_ADVANCE)
	{
		LOG_ERROR << "[PlayerInbox] Fetch reply carries items under unexpected fence=" << static_cast<int>(fence)
				  << " player_id=" << playerId << " domain=" << domain;
		Defer(*state, playerId, PlayerInboxDeferReason::kServerError, nowMs);
		return;
	}
	if (const std::string malformed = DescribeMalformedBatch(response); !malformed.empty())
	{
		Halt(*state, playerId, PlayerInboxHaltReason::kBatchMalformed, malformed);
		FinishRound(*state, nowMs);
		return;
	}

	state->hasMore = response.has_more();
	state->pendingItems.assign(response.items().begin(), response.items().end());
	state->phase = PlayerInboxPhase::kApplying;
	// 回包到达、准备应用之前再判一次前置闸(在 ApplyPending 里)。
	ApplyPending(player, playerId, *state, nowMs);
}

void PlayerInboxSystem::OnFetchFailed(const player_inbox::FetchPlayerInboxRequest &request, uint32_t grpcCode,
									  const std::string &detail)
{
	const uint64_t playerId = request.player_id();
	const uint32_t domain = static_cast<uint32_t>(request.domain());
	entt::entity player = entt::null;
	auto *state = FindLiveState(playerId, domain, player);
	if (state == nullptr || state->phase != PlayerInboxPhase::kFetching || request.correlation_id() == 0 ||
		state->correlationId != request.correlation_id())
	{
		LOG_DEBUG << "[PlayerInbox] Fetch failure discarded (entity gone or request superseded) player_id=" << playerId
				  << " code=" << grpcCode;
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	state->phase = PlayerInboxPhase::kIdle;
	state->correlationId = 0;
	state->inflightSinceMs = 0;
	if (grpcCode == kGrpcUnimplemented)
	{
		// 收件箱还是旧版本:长退避,记 INFO,不计故障(设计 §3.3、§7.6)。
		if (state->deferReason != PlayerInboxDeferReason::kUnimplemented)
		{
			LOG_INFO << "[PlayerInbox] Inbox service does not implement FetchItems yet (older version); backing off"
					 << " player_id=" << playerId << " domain=" << domain;
		}
		ScheduleRetry(*state, playerId, PlayerInboxDeferReason::kUnimplemented,
					  player_inbox_rules::kUnimplementedBackoffMs, nowMs);
		return;
	}
	if (state->deferReason != PlayerInboxDeferReason::kTransport)
	{
		LOG_WARN << "[PlayerInbox] Fetch failed (result unknown) player_id=" << playerId << " domain=" << domain
				 << " code=" << grpcCode << " detail=" << detail;
	}
	Defer(*state, playerId, PlayerInboxDeferReason::kTransport, nowMs);
}

void PlayerInboxSystem::OnAckReply(const player_inbox::AckPlayerInboxResponse &response)
{
	const uint64_t playerId = response.player_id();
	const uint32_t domain = static_cast<uint32_t>(response.domain());
	entt::entity player = entt::null;
	auto *state = FindLiveState(playerId, domain, player);
	if (state == nullptr || state->phase != PlayerInboxPhase::kAcking || response.correlation_id() == 0 ||
		state->correlationId != response.correlation_id())
	{
		LOG_DEBUG << "[PlayerInbox] Ack reply discarded (entity gone, recreated or request superseded) player_id="
				  << playerId << " domain=" << domain << " corr=" << response.correlation_id();
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	state->phase = PlayerInboxPhase::kAwaitDurable;
	state->correlationId = 0;
	state->inflightSinceMs = 0;

	const auto fence = response.fence();
	if (fence == player_inbox::PLAYER_INBOX_FENCE_STALE)
	{
		// 新 owner 已经推进了围栏。没有关系:它加载的存档里已有这些结局,会自己补确认(设计 §7.7、§7.10)。
		LOG_WARN << "[PlayerInbox] Ack fenced off as STALE; the newer owner re-acknowledges from its persisted ledger"
				 << " player_id=" << playerId << " domain=" << domain << " decisions=" << state->ackQueue.size();
		state->ackQueue.clear();
		FinishRound(*state, nowMs);
		ScheduleRetry(*state, playerId, PlayerInboxDeferReason::kStaleOwner, player_inbox_rules::kStaleOwnerBackoffMs,
					  nowMs);
		return;
	}
	if (fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_REQUIRED || fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_AHEAD)
	{
		state->ackQueue.clear();
		Halt(*state, playerId,
			 fence == player_inbox::PLAYER_INBOX_FENCE_EPOCH_REQUIRED ? PlayerInboxHaltReason::kEpochRequired
																	   : PlayerInboxHaltReason::kEpochAhead,
			 "op=ack");
		FinishRound(*state, nowMs);
		return;
	}
	if (const uint32_t tip = response.error_message().id(); !IsTipOk(tip))
	{
		// 服务端故障:保留 ackQueue,退避重发(设计 §7.7)。
		if (state->ackFailStreak < std::numeric_limits<uint32_t>::max())
		{
			++state->ackFailStreak;
		}
		state->nextAckMs = nowMs + player_inbox_rules::BackoffMs(state->ackFailStreak, JitterPermille());
		LOG_WARN << "[PlayerInbox] Ack answered with tip=" << tip << " player_id=" << playerId << " domain=" << domain
				 << " streak=" << state->ackFailStreak;
		return;
	}

	// 逐条结果:FINALIZED / ALREADY_SAME 视为成功;其余全是协议异常 —— 记 ERROR、停用、不重试(设计 §7.7)。
	std::string anomaly;
	if (response.results_size() != static_cast<int>(state->ackQueue.size()))
	{
		anomaly = "results=" + std::to_string(response.results_size()) +
				  " decisions=" + std::to_string(state->ackQueue.size());
	}
	else
	{
		for (int i = 0; i < response.results_size(); ++i)
		{
			const auto &result = response.results(i);
			const auto &decision = state->ackQueue[static_cast<std::size_t>(i)];
			if (result.seq() != decision.seq())
			{
				anomaly = "result[" + std::to_string(i) + "] seq=" + std::to_string(result.seq()) +
						  " expected=" + std::to_string(decision.seq());
				break;
			}
			if (result.result() != player_inbox::PLAYER_INBOX_ACK_FINALIZED &&
				result.result() != player_inbox::PLAYER_INBOX_ACK_ALREADY_SAME)
			{
				anomaly = "seq=" + std::to_string(result.seq()) + " result=" +
						  player_inbox::PlayerInboxAckResult_Name(result.result()) +
						  " state=" + player_inbox::PlayerInboxItemState_Name(result.state());
				break;
			}
		}
	}
	if (!anomaly.empty())
	{
		LOG_ERROR << "[PlayerInbox] metric=inbox_ack_anomaly player_id=" << playerId << " domain=" << domain
				  << " detail=" << anomaly;
		state->ackQueue.clear();
		Halt(*state, playerId, PlayerInboxHaltReason::kAckAnomaly, anomaly);
		FinishRound(*state, nowMs);
		return;
	}

	LOG_DEBUG << "[PlayerInbox] Acknowledged " << state->ackQueue.size() << " decision(s) player_id=" << playerId
			  << " domain=" << domain;
	state->ackQueue.clear();
	state->ackFailStreak = 0;
	FinishRound(*state, nowMs);
}

void PlayerInboxSystem::OnAckFailed(const player_inbox::AckPlayerInboxRequest &request, uint32_t grpcCode,
									const std::string &detail)
{
	const uint64_t playerId = request.player_id();
	const uint32_t domain = static_cast<uint32_t>(request.domain());
	entt::entity player = entt::null;
	auto *state = FindLiveState(playerId, domain, player);
	if (state == nullptr || state->phase != PlayerInboxPhase::kAcking || request.correlation_id() == 0 ||
		state->correlationId != request.correlation_id())
	{
		LOG_DEBUG << "[PlayerInbox] Ack failure discarded (entity gone or request superseded) player_id=" << playerId
				  << " code=" << grpcCode;
		return;
	}
	const uint64_t nowMs = gNowMsFn();
	state->phase = PlayerInboxPhase::kAwaitDurable;
	state->correlationId = 0;
	state->inflightSinceMs = 0;
	// Ack 请求失败:保留 ackQueue,退避重发。实体若在这期间被销毁,下次落地时 Fetch 会再次返回这些 PENDING 条目,
	// 届时从账本补确认(设计 §7.7)。
	if (grpcCode == kGrpcUnimplemented)
	{
		state->nextAckMs = nowMs + player_inbox_rules::kUnimplementedBackoffMs;
		LOG_INFO << "[PlayerInbox] Inbox service does not implement AckItems yet (older version); backing off"
				 << " player_id=" << playerId << " domain=" << domain;
		return;
	}
	if (state->ackFailStreak < std::numeric_limits<uint32_t>::max())
	{
		++state->ackFailStreak;
	}
	state->nextAckMs = nowMs + player_inbox_rules::BackoffMs(state->ackFailStreak, JitterPermille());
	LOG_WARN << "[PlayerInbox] Ack failed (result unknown; will resend) player_id=" << playerId << " domain=" << domain
			 << " code=" << grpcCode << " detail=" << detail << " streak=" << state->ackFailStreak;
}

// ── 测试注入点 ──────────────────────────────────────────────────────────────

void PlayerInboxSystem::SetPersistFnForTest(PersistFn fn)
{
	gPersistFn = (fn == nullptr) ? &PlayerLifecycleSystem::SavePlayerToRedis : fn;
}

void PlayerInboxSystem::SetNowFnForTest(NowMsFn fn)
{
	gNowMsFn = (fn == nullptr) ? &DefaultSteadyNowMs : fn;
}

void PlayerInboxSystem::SetTransportForTest(SendFetchFn sendFetch, SendAckFn sendAck)
{
	gSendFetchFn = (sendFetch == nullptr) ? &DefaultSendFetch : sendFetch;
	gSendAckFn = (sendAck == nullptr) ? &DefaultSendAck : sendAck;
}

void PlayerInboxSystem::SetConsumerConfigForTest(bool overrideConfig, bool consumerEnabled, uint32_t periodicFetchMinutes)
{
	gConfigOverride.active = overrideConfig;
	gConfigOverride.consumerEnabled = consumerEnabled;
	gConfigOverride.periodicFetchMinutes = periodicFetchMinutes;
}
