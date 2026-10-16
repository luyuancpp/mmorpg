#include "node_utils.h"
#include "grpc_client_runtime.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include "muduo/base/Logging.h"
#include "muduo_grpc/channel.h"
#include <thread_context/node_context_manager.h>

namespace
{
// 这里只发布所属对象的地址;内容与引用寿命仍由 loop 线程归属和停机排空保证。
std::atomic<const NodeInfo *> localNodeInfo{nullptr};
}

const NodeInfo *InstallLocalNodeInfo(const NodeInfo *info)
{
	return localNodeInfo.exchange(info, std::memory_order_acq_rel);
}

const NodeInfo &GetNodeInfo()
{
	const auto *info = localNodeInfo.load(std::memory_order_acquire);
	return info != nullptr ? *info : NodeInfo::default_instance();
}

uint32_t GetZoneId()
{
	return GetNodeInfo().zone_id();
}

namespace
{
	// 分四级挑,前一级有实例就不看后一级(枚举顺序即挑选顺序):
	//
	//   ①  READY
	//   ②  IDLE / CONNECTING
	//   ③a 挂了通道、状态为 TRANSIENT_FAILURE / SHUTDOWN
	//   ③b 没挂通道
	//
	// 底线:注册表里只要有一个 SceneManager 实体就一定挑出一个,与旧实现(playerId % 已注册实例数)一样,
	// 只在注册表为空时返回 null。分级只做"有更好的选择时避开坏通道",不新增 null 窗口 —— 调用方拿到 null
	// 走的是立即失败分支,它们比"挑一个坏实例、让那次调用失败"更伤:跨 zone 交接在 handoff 标记 SET 已发出后
	// 拿不到 SceneManager 只能 ConcludeHandoffAfterMarkSent(销毁 + 踢线重登);而挑一个坏实例,那次调用以
	// UNAVAILABLE 进失败处理器(grpc-client-deadline-failure-callback.md §5)或由 30s 看门狗收尾,
	// 这正是改造前的行为。
	//
	// 为什么 ② 算可用,而不是像 PickReadyDataServiceNode 那样只认 READY:
	// - 只认 READY 会在两种正常情形下一个都挑不到:
	//   1) 通道刚建好时在 CONNECTING。ConnectToGrpcNode 建好即 GetState(true) 预连,收到服务端首个 SETTINGS
	//      才转 READY(grpc-muduo-transport.md §5),发现实例后的头一个往返里一个 READY 都没有;
	//   2) READY 的连接断开(对端重启、GOAWAY、网络闪断)后通道回 IDLE,下一次调用或 GetState(true) 才重连
	//      (与 pick_first 一致)。旧官方通道「空闲 30 分钟回 IDLE」已随第 3 批消失(§14.2),这一条仍在。
	//   号段传输只认 READY 没事,因为它的调用方按退避重试;SceneManager 的调用方不能拿"刚启动 / 连接刚断"
	//   当失败。
	//
	// 为什么 ③a 排在 ② 之后:实例崩溃后连接断开,通道先回 IDLE;这里的 GetState(true) 把它踢去重连,对端拒连
	// (或建连超时)就进 TRANSIENT_FAILURE,并且 muduo_grpc::Channel 在退避重连期间对外一直报 TRANSIENT_FAILURE、
	// 不会回报 CONNECTING,直到真的连上(muduo_grpc/channel.cpp 的 OnBackoffTimer,与 pick_first 一致)。这个状态下
	// 的调用立即以 UNAVAILABLE 失败(Channel::Dispatch,不做 wait_for_ready),有 ①② 时挑它等于白白失败;
	// etcd 租约 60s 到期前死实例仍在注册表里,这正是要避开的窗口。只有 ①② 全空时才兜底挑它,结果与改造前相同。
	//
	// 纯选择器仍能描述无通道输入，供独立排序用例使用；生产适配层只传有完整传输的候选。
	// 无通道实体与真实连接暂时不可达不是同一状态：前者立即拒绝，后者仍走原有失败回调。
	// 入站握手同时拒绝纯 gRPC 类型，防止其按 uuid 抢占出站实体。
	//
	// 残余窗口:实例刚崩、还没被踢过时它显示 IDLE。只有在一个 READY 都没有时它才可能被挑中,那一次请求
	// 以 UNAVAILABLE 走失败处理器,之后它就是 TRANSIENT_FAILURE。对端主机整个消失(没有 RST)时,重连要等到
	// 连接超时(ChannelOptions::connectTimeout,默认 20s),这段时间它停在 CONNECTING,同样排在 READY 之后。
	// muduo 的「连接失败」回调落地前(grpc-muduo-transport.md §5「已知差异」、§14.1),对端拒连也是这样:
	// muduo Connector 在内部静默重试,要等到连接超时才进 TRANSIENT_FAILURE。
	enum class SelectionTier : uint8_t
	{
		kReady,               // ①
		kConnectable,         // ②
		kLastResortFailed,    // ③a
		kLastResortNoChannel, // ③b
		kCount,
	};
	constexpr std::size_t kSelectionTierCount = static_cast<std::size_t>(SelectionTier::kCount);

	// 只用于兜底告警里标出挑中的是哪一级。顺序必须与 SelectionTier 逐项对应(static_assert 只拦数量不一致)。
	constexpr const char *kTierNames[] = {
		"ready",
		"idle_or_connecting",
		"failed_channel",
		"no_channel",
	};
	static_assert(std::size(kTierNames) == kSelectionTierCount,
				  "kTierNames 必须与 SelectionTier 一一对应:加了枚举项就得加名字");

	SelectionTier ClassifyCandidate(const scene_manager_selector::Candidate &candidate)
	{
		if (!candidate.channelState)
		{
			return SelectionTier::kLastResortNoChannel;
		}
		switch (*candidate.channelState)
		{
		case muduo_grpc::ConnectivityState::kReady:
			return SelectionTier::kReady;
		case muduo_grpc::ConnectivityState::kIdle:
		case muduo_grpc::ConnectivityState::kConnecting:
			return SelectionTier::kConnectable;
		case muduo_grpc::ConnectivityState::kTransientFailure:
		case muduo_grpc::ConnectivityState::kShutdown:
		default:
			return SelectionTier::kLastResortFailed;
		}
	}

	const char *TierName(SelectionTier tier)
	{
		return kTierNames[static_cast<std::size_t>(tier)];
	}

	// 取模前先把 playerId 打散(splitmix64 的终混步)。PlayerId 是 bwmarrin 布局
	// (41 位毫秒 | 13 位 node | 9 位 step),低 9 位是毫秒内序号、每个新毫秒归零,低频建角时几乎全是 0。
	// 直接 playerId % 实例数,实例数为 2 / 4 / 8 时几乎所有玩家都压在第 0 个实例上(旧实现就是这样)。
	uint64_t SpreadPlayerId(Guid playerId)
	{
		uint64_t z = playerId + 0x9e3779b97f4a7c15ull;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	}

	entt::entity PickWithinTier(const std::vector<scene_manager_selector::Candidate> &candidates, SelectionTier tier,
								Guid playerId)
	{
		std::size_t tierSize = 0;
		for (const auto &candidate : candidates)
		{
			if (ClassifyCandidate(candidate) == tier)
			{
				++tierSize;
			}
		}
		if (tierSize == 0)
		{
			return entt::null;
		}

		auto remaining = SpreadPlayerId(playerId) % tierSize;
		for (const auto &candidate : candidates)
		{
			if (ClassifyCandidate(candidate) != tier)
			{
				continue;
			}
			if (remaining == 0)
			{
				return candidate.entity;
			}
			--remaining;
		}
		return entt::null;
	}

	// 落到 ③a / ③b 时的告警:说明此刻没有任何能用的 SceneManager 通道,本次调用多半以 UNAVAILABLE 失败
	// (③b 只出现在纯函数用例里:生产适配层不传无通道实体;真发给无通道实体,生成的 Send 也只是本地 UNAVAILABLE)。
	// 全挂期间每次换图都会走到这里,所以要节流:每 10s 最多一行,期间压掉的次数带在下一行里,排障时仍能看出频率。
	//
	// 线程模型:GetSceneManagerEntity 读的是 thread_local 的 tlsNodeContextManager,只在拥有它的节点主循环线程上
	// 调用(调用方都是 scene 主循环里的换图 / 跨 zone 交接 / 组队跟随 / RPC 应答处理;gRPC 应答由 muduo_grpc::Channel
	// 在通道所属的主循环上 queueInLoop 投递,见 muduo_grpc/channel.h)。节流状态与注册表同样做成 thread_local,
	// 不需要锁;每个进程只有这一个线程调它,"每线程 10s 一行"就是"每进程 10s 一行"。
	// 用 steady_clock:节流间隔不能受墙钟跳变影响。
	constexpr auto kLastResortWarnInterval = std::chrono::seconds(10);

	void WarnLastResortPick(const std::vector<scene_manager_selector::Candidate> &candidates, entt::entity picked,
							SelectionTier pickedTier)
	{
		struct WarnThrottle
		{
			std::optional<std::chrono::steady_clock::time_point> lastWarnAt;
			uint64_t suppressedSinceLastWarn{0};
		};
		static thread_local WarnThrottle throttle;

		const auto now = std::chrono::steady_clock::now();
		if (throttle.lastWarnAt && now - *throttle.lastWarnAt < kLastResortWarnInterval)
		{
			++throttle.suppressedSinceLastWarn;
			return;
		}

		// 走到兜底级说明 ①② 都是空的,READY / IDLE / CONNECTING 计数必为 0,不再打印。
		std::size_t transientFailure = 0;
		std::size_t shutdown = 0;
		std::size_t noChannel = 0;
		for (const auto &candidate : candidates)
		{
			if (!candidate.channelState)
			{
				++noChannel;
			}
			else if (*candidate.channelState == muduo_grpc::ConnectivityState::kTransientFailure)
			{
				++transientFailure;
			}
			else if (*candidate.channelState == muduo_grpc::ConnectivityState::kShutdown)
			{
				++shutdown;
			}
		}
		LOG_WARN << "[SceneManagerSelect] no READY / IDLE / CONNECTING SceneManager instance, last-resort pick:"
				 << " registered=" << candidates.size()
				 << " transient_failure=" << transientFailure
				 << " shutdown=" << shutdown
				 << " no_channel=" << noChannel
				 << " picked_entity=" << entt::to_integral(picked)
				 << " picked_tier=" << TierName(pickedTier)
				 << " suppressed_since_last_warn=" << throttle.suppressedSinceLastWarn;
		throttle.lastWarnAt = now;
		throttle.suppressedSinceLastWarn = 0;
	}
}

namespace scene_manager_selector
{
	// 同级取模而不是按玩家粘滞(rendezvous 哈希):go/scene_manager 里进程内的状态全都按场景节点记
	// (knownNodes / nodeGoneObservedAt / pendingDeadNodes / deferredNodeDetaches / nodeConnCache),
	// 按玩家的状态(request_id 去重、owner_epoch、交接标记)都在 Redis 里,各实例共享。节点维度的本地观察
	// 本来就各副本各记一份,设计上已按"本副本没看到就退回 Redis 的 death_at"兜底,与玩家落在哪个实例无关。
	// 所以候选集变化时"大多数玩家换实例"没有正确性代价,不值得多一层哈希。
	// 顺序上也没有损失:即使同一实例,gRPC 服务端也是每个调用一个 goroutine,本来就不保证两次请求的先后。
	// 若日后 scene_manager 加了按玩家驻留在进程内存里的状态,这里要改成 rendezvous 哈希。
	entt::entity Pick(const std::vector<Candidate> &candidates, Guid playerId)
	{
		// 按枚举顺序逐级挑,前一级有实例就不看后一级。
		for (std::size_t tierIndex = 0; tierIndex < kSelectionTierCount; ++tierIndex)
		{
			const auto tier = static_cast<SelectionTier>(tierIndex);
			if (const auto picked = PickWithinTier(candidates, tier, playerId); picked != entt::null)
			{
				return picked;
			}
		}
		// 四级覆盖了所有候选,只有空表会走到这里。
		return entt::null;
	}
}

entt::entity GetSceneManagerEntity(Guid playerId)
{
	auto &smRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneManagerNodeService);

	// 仅把真正拥有传输、且尚未关闭的实例交给选择器。无通道的握手实体不代表服务可用。
	// 实例数是个位数,每次现拼一张候选表;调用频率跟着玩家换图走,不是热路径。
	std::vector<scene_manager_selector::Candidate> candidates;
	for (const auto entity : smRegistry.view<NodeInfo>())
	{
		if (!HasUsableGrpcClient(smRegistry, entity))
		{
			continue;
		}
		scene_manager_selector::Candidate candidate{entity};
		if (const auto channel = FindGrpcChannel(smRegistry, entity); channel != nullptr)
		{
			candidate.channelState = channel->GetState(/*tryToConnect=*/true);
		}
		else
		{
			continue;
		}
		candidates.push_back(candidate);
	}

	const auto picked = scene_manager_selector::Pick(candidates, playerId);
	if (picked == entt::null)
	{
		// 没有具备完整传输的 SceneManager。调用方记录自己的上下文。
		return entt::null;
	}

	for (const auto &candidate : candidates)
	{
		if (candidate.entity != picked)
		{
			continue;
		}
		if (const auto tier = ClassifyCandidate(candidate);
			tier == SelectionTier::kLastResortFailed || tier == SelectionTier::kLastResortNoChannel)
		{
			WarnLastResortPick(candidates, picked, tier);
		}
		break;
	}
	return picked;
}
