#include "db_task_dispatch_system.h"

#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

#include "engine/infra/messaging/kafka/kafka_producer.h"
#include "muduo/base/Logging.h"

namespace db_task_dispatch_stats
{
	namespace
	{
		std::array<std::atomic<std::uint64_t>, kCounterCount> &Counters()
		{
			static std::array<std::atomic<std::uint64_t>, kCounterCount> g_counters{};
			return g_counters;
		}
	} // namespace

	void Inc(Counter counter)
	{
		const auto index = static_cast<std::size_t>(counter);
		if (index < kCounterCount)
		{
			Counters()[index].fetch_add(1, std::memory_order_relaxed);
		}
	}

	Snapshot Read()
	{
		Snapshot snapshot;
		const auto &counters = Counters();
		for (std::size_t i = 0; i < kCounterCount; ++i)
		{
			snapshot.values[i] = counters[i].load(std::memory_order_relaxed);
		}
		return snapshot;
	}
} // namespace db_task_dispatch_stats

namespace
{
	using db_task_dispatch_stats::Counter;

	// 本线程(scene 主循环)的全部记账状态。函数内 thread_local:不同线程互不可见,也不参与静态初始化顺序。
	struct State
	{
		db_task_dispatch::Ledger ledger;
		// 回执监听器压进来、等 Pump 在 loop 顶层处理的玩家号(可能重复,Pump 去重)。
		std::vector<Guid> pending;
		DbTaskDispatchSystem::SettledHandler onSettled;
		// 回执监听器已登记。没登记时跟踪了也等不到回执,SendDbTask 退化为不跟踪的 send()。
		bool initialized{false};
	};

	State &GetState()
	{
		thread_local State state;
		return state;
	}
} // namespace

void DbTaskDispatchSystem::Initialize(SettledHandler onSettled)
{
	auto &state = GetState();
	state.onSettled = std::move(onSettled);
	state.initialized = true;
	// 无捕获 lambda,转调静态函数:不绑任何对象(AGENTS §11.7)。在 dr_cb / 重建栈里同步执行,只记账。
	KafkaProducer::Instance().setDeliveryListener([](const KafkaProducer::DeliveryReport &report)
												  { OnDeliveryReport(report.correlationId, report.err == RdKafka::ERR_NO_ERROR); });
}

void DbTaskDispatchSystem::Shutdown()
{
	auto &state = GetState();
	if (!state.initialized)
	{
		return; // 幂等:RedisSystem 析构里的第二次调用走这里
	}
	state.initialized = false;
	KafkaProducer::Instance().setDeliveryListener(nullptr);

	// 停机时还没落定的玩家:在途回执来不及等,或投递失败没能重存。Redis 里有最新数据(下次登录自愈),
	// 但合服、回档这类以 MySQL 为准的操作会读到旧档 —— 这行就是事后追查的证据。
	if (const std::size_t unsettled = state.ledger.UnsettledPlayerCount(); unsettled > 0)
	{
		const auto ids = state.ledger.UnsettledPlayers(kShutdownUnsettledLogLimit);
		std::string idList;
		for (std::size_t i = 0; i < ids.size(); ++i)
		{
			if (i != 0)
			{
				idList += ',';
			}
			idList += std::to_string(ids[i]);
		}
		LOG_ERROR << "[DbTaskDispatch] shutting down with " << unsettled
				  << " player(s) whose MySQL copy may lag Redis (in-flight or undelivered DB write tasks): ids=[" << idList
				  << "]" << (unsettled > ids.size() ? " (truncated)" : "");
	}
	state.onSettled = nullptr;
	state.pending.clear();
}

db_task_dispatch::SaveGeneration DbTaskDispatchSystem::BeginSave(Guid playerId)
{
	return GetState().ledger.BeginSave(playerId);
}

DbTaskDispatchSystem::SendResult DbTaskDispatchSystem::SendDbTask(Guid playerId,
																  db_task_dispatch::SaveGeneration generation,
																  const std::string &topic, const std::string &payload,
																  const std::string &key)
{
	auto &state = GetState();
	SendResult result;

	if (!state.initialized)
	{
		// 回执监听器没登记(初始化顺序被破坏;生产上 RedisSystem::Initialize 早于任何存盘):跟踪了也等不到回执,
		// 在途数永不归零,下线收尾会一直等到票据超时。退化为不跟踪的 send():同步失败照样记成本代失败,
		// 异步失败无从得知(即改动前的行为)。
		const RdKafka::ErrorCode err = KafkaProducer::Instance().send(topic, payload, key);
		if (err != RdKafka::ERR_NO_ERROR)
		{
			state.ledger.MarkLocalFailure(playerId, generation);
			db_task_dispatch_stats::Inc(Counter::kProduceRejected);
			result.error = RdKafka::err2str(err);
			return result;
		}
		result.queued = true;
		return result;
	}

	// 先领票再发:回执可能就在 sendTracked 内部的 poll 里派发,那时票据必须已在账上。
	// sendTracked 期间监听器可能同步改账本(别的票据的回执),state 是 thread_local 的引用,不会失效。
	const db_task_dispatch::Ticket ticket =
		state.ledger.IssueTicket(playerId, generation, db_task_dispatch::Clock::now());
	const RdKafka::ErrorCode err = KafkaProducer::Instance().sendTracked(topic, payload, key, ticket);
	if (err != RdKafka::ERR_NO_ERROR)
	{
		// 没入队 = 永远不会有回执(kafka_producer.h「逐条回执契约」):立即销票、记失败。
		state.ledger.OnProduceRejected(ticket);
		db_task_dispatch_stats::Inc(Counter::kProduceRejected);
		result.error = RdKafka::err2str(err);
		return result;
	}
	result.queued = true;
	return result;
}

void DbTaskDispatchSystem::MarkLocalFailure(Guid playerId, db_task_dispatch::SaveGeneration generation)
{
	GetState().ledger.MarkLocalFailure(playerId, generation);
	db_task_dispatch_stats::Inc(Counter::kLocalFailure);
}

void DbTaskDispatchSystem::FinishSave(Guid playerId, db_task_dispatch::SaveGeneration generation, bool allQueued)
{
	GetState().ledger.FinishSave(playerId, generation, allQueued);
}

bool DbTaskDispatchSystem::NeedsResave(Guid playerId)
{
	return GetState().ledger.NeedsResave(playerId);
}

std::uint32_t DbTaskDispatchSystem::InFlight(Guid playerId)
{
	return GetState().ledger.InFlight(playerId);
}

void DbTaskDispatchSystem::OnPlayerGone(Guid playerId)
{
	const auto result = GetState().ledger.ForgetIfNoInFlight(playerId);
	if (result.forgotten && result.hadUnresolvedFailure)
	{
		db_task_dispatch_stats::Inc(Counter::kOrphanUnsynced);
		LOG_WARN << "[DbTaskDispatch] player " << playerId
				 << " left this node with an undelivered DB write task; MySQL lags Redis until the next owner saves this"
				 << " player (metric=db_task_orphan_unsynced)";
	}
}

void DbTaskDispatchSystem::OnDeliveryReport(db_task_dispatch::Ticket ticket, bool delivered)
{
	// 在 KafkaProducer 的 dr_cb / 重建栈里同步执行:只改账本、计数、压待办。不碰 ECS(组件集合此刻可能正被
	// 调用 send() 的那段代码持有引用),也不打逐条日志(生产者自己已按限流记了失败)。
	auto &state = GetState();
	const db_task_dispatch::DeliveryOutcome outcome = state.ledger.OnDelivery(ticket, delivered);
	switch (outcome.effect)
	{
	case db_task_dispatch::DeliveryEffect::kDelivered:
		break;
	case db_task_dispatch::DeliveryEffect::kFailedSuperseded:
		db_task_dispatch_stats::Inc(Counter::kDeliveryFailed);
		db_task_dispatch_stats::Inc(Counter::kDeliveryFailedSuperseded);
		break;
	case db_task_dispatch::DeliveryEffect::kFailedNeedsResave:
		db_task_dispatch_stats::Inc(Counter::kDeliveryFailed);
		break;
	case db_task_dispatch::DeliveryEffect::kUnknownTicket:
	case db_task_dispatch::DeliveryEffect::kCount:
	default:
		return; // 重复 / 迟到 / 条目已回收:账本没变,不需要任何人再看
	}
	if (outcome.inFlightAfter == 0 || outcome.effect == db_task_dispatch::DeliveryEffect::kFailedNeedsResave)
	{
		state.pending.push_back(outcome.playerId);
	}
}

void DbTaskDispatchSystem::Pump()
{
	// 1. 派发回执。回执只在本线程的 poll / flush / send 里派发,空闲节点上 send() 不来,回执就一直压着
	//    (下线收尾在等它)。producer 还没建出来时是空操作。
	KafkaProducer::Instance().poll();

	auto &state = GetState();

	// 2. 票据超时兜底:330s 内一条回执都没有,按投递失败处理。迟到的真回执到时票据已销,OnDelivery 返回 kUnknownTicket。
	const auto expired = state.ledger.ExpireStale(db_task_dispatch::Clock::now(), kTicketTimeout);
	std::size_t expiredCount = 0;
	Guid firstExpiredPlayer = kInvalidGuid;
	for (const auto &outcome : expired)
	{
		if (outcome.effect == db_task_dispatch::DeliveryEffect::kUnknownTicket)
		{
			continue;
		}
		db_task_dispatch_stats::Inc(Counter::kTicketExpired);
		if (expiredCount == 0)
		{
			firstExpiredPlayer = outcome.playerId;
		}
		++expiredCount;
		state.pending.push_back(outcome.playerId);
	}
	if (expiredCount > 0)
	{
		LOG_ERROR << "[DbTaskDispatch] " << expiredCount << " DB write task(s) got no Kafka delivery report within "
				  << kTicketTimeout.count() << "s; treating them as undelivered (first player=" << firstExpiredPlayer
				  << ", metric=db_task_ticket_expired)";
	}

	// 3. 待办玩家逐个交给生命周期。先换出来再处理:onSettled 里的重存会再发 DBTask,其回执(若在本栈里派发)
	//    压进的是新的待办表,留给下一拍,不会在这里无限循环。
	if (state.pending.empty())
	{
		return;
	}
	std::vector<Guid> settled;
	settled.swap(state.pending);
	std::sort(settled.begin(), settled.end());
	settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
	const SettledHandler handler = state.onSettled; // 拷一份:处理期间即使有人注销,本轮也不悬空
	if (!handler)
	{
		return;
	}
	for (const Guid playerId : settled)
	{
		handler(playerId);
	}
}

std::size_t DbTaskDispatchSystem::UnsettledPlayerCount()
{
	return GetState().ledger.UnsettledPlayerCount();
}

std::size_t DbTaskDispatchSystem::InFlightTickets()
{
	return GetState().ledger.OutstandingTickets();
}

db_task_dispatch::Ledger &DbTaskDispatchSystem::LedgerForTest()
{
	return GetState().ledger;
}

void DbTaskDispatchSystem::ResetForTest()
{
	auto &state = GetState();
	state.ledger.Clear();
	state.pending.clear();
}
