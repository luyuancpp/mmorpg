#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <string>

#include "engine/core/type_define/type_define.h" // Guid
#include "services/scene/player/system/db_task_dispatch_ledger.h"

// scene 存盘 DBTask 投递记账的粘合层:把 db_task_dispatch::Ledger(纯账本)接到 KafkaProducer 的逐条回执
// (sendTracked + setDeliveryListener)与玩家生命周期上。头文件刻意不 include rdkafka。
//
// 职责边界:
//   * SavePlayerToRedisImpl 每次存盘 BeginSave → 每条 DBTask SendDbTask(失败时 MarkLocalFailure)→ FinishSave;
//     dirty-save 快路径问 NeedsResave,有欠账就强制按当前内存整份重写(绝不重放旧载荷)。
//   * 回执监听器在 KafkaProducer 的 dr_cb / 重建栈里同步执行(OnDeliveryReport):只改账本、只把玩家号压进待办,
//     **不碰 ECS、不打逐条日志**。回执与同步拒绝的错误码都原样进账本,按 kafka_producer_policy::IsPermanentMessageError
//     分成可重试 / 永久两种(PermanentFailure):永久失败时重存没有收益,下线收尾不再为它保留实体。
//   * Pump 由 RedisSystem 的 50ms 定时器在 loop 顶层驱动:先 poll() 派发回执(空闲节点上 send() 不来,回执就没人派发),
//     再做票据超时兜底,最后把待办玩家逐个交给 Initialize 时登记的 onSettled(PlayerLifecycleSystem::OnDbTaskDispatchSettled)。
//   * 实体离开本节点时(DestroyPlayer,或 onSettled 发现实体已不在)调 OnPlayerGone 回收条目;离开时还欠 MySQL 一次写的
//     计 orphan_unsynced 并打 WARN —— 新属主的首存一定写盘,MySQL 会自愈,在那之前只有 Redis 是最新的。
//
// 回调生命周期(AGENTS §11.7):监听器与 Pump 定时器都是无捕获 lambda,转调本类静态函数,不绑 this / 裸指针。
// Shutdown 先注销监听器;KafkaProducer 析构时第一件事也是注销监听器,不依赖 thread_local 的析构顺序。
// 停机收账的时刻:Node 的拆除前钩子(Node::SetBeforeTeardown → RedisSystem::StopDbTaskDispatch)先停 Pump 再调 Shutdown,
// 那时 producer 最后一次有界 flush 已计入账本、日志仍在写文件;main.cpp 在 Node 析构之后的那次调用只是幂等兜底。
// 线程模型:全部状态是本 .cpp 里的函数内 thread_local,只在 scene 主循环线程上读写。

// 进程级计数(写法同 owner_epoch_stats:relaxed atomic,不接 Prometheus,玩家号绝不做 label)。
// RedisSystem 的 30s 定时器打成一行 [DbTaskDispatch](key=value 平铺的累计值)。稳态下除 exit_await_ack、
// delivery_failed_superseded 外都应为 0:
//   produce_rejected            DBTask 同步发送被拒(没入队)
//   local_failure               DBTask 序列化失败,根本没走到发送
//   delivery_failed             投递回执失败(含重建时合成的失败);其中被更新的整份存盘盖过的另计在下一项
//   delivery_failed_superseded  上一项里已被更新的整份入队存盘盖过、不需要重存的部分
//   ticket_expired              330s 内没有任何回执,按失败处理(超时兜底)
//   forced_resave               dirty-save 快路径本可跳过,因欠 MySQL 一次写而强制真写
//   exit_await_ack              下线收尾在等 DBTask 回执(每次退出至多计一次;正常现象)
//   exit_resave                 下线收尾因投递失败重存
//   exit_resave_capped          下线收尾重存到上限仍投递失败,保留实体(每次判定计一次)
//   exit_rekick                 被保留的实体又收到退出请求,重新开始重存
//   exit_unsynced               下线收尾带着 MySQL 落后收尾:交接已发起、不得再重存;或欠账的那次失败是永久性的
//                               (消息过大 / 无写权限等,重存必然失败,不再保留实体)
//   orphan_unsynced             实体离开本节点时仍欠 MySQL 一次写(含 exit_unsynced 那一部分;被废黜、交接放行等路径)
namespace db_task_dispatch_stats
{
	enum class Counter : std::uint8_t
	{
		kProduceRejected,
		kLocalFailure,
		kDeliveryFailed,
		kDeliveryFailedSuperseded,
		kTicketExpired,
		kForcedResave,
		kExitAwaitAck,
		kExitResave,
		kExitResaveCapped,
		kExitRekick,
		kExitUnsynced,
		kOrphanUnsynced,

		kCount
	};

	inline constexpr std::size_t kCounterCount = static_cast<std::size_t>(Counter::kCount);

	inline constexpr const char *kCounterNames[] = {
		"produce_rejected",
		"local_failure",
		"delivery_failed",
		"delivery_failed_superseded",
		"ticket_expired",
		"forced_resave",
		"exit_await_ack",
		"exit_resave",
		"exit_resave_capped",
		"exit_rekick",
		"exit_unsynced",
		"orphan_unsynced",
	};
	static_assert(std::size(kCounterNames) == kCounterCount, "kCounterNames 必须与 Counter 一一对应");

	void Inc(Counter counter);

	struct Snapshot
	{
		std::array<std::uint64_t, kCounterCount> values{};

		bool operator==(const Snapshot &) const = default;

		std::uint64_t Get(Counter counter) const
		{
			const auto index = static_cast<std::size_t>(counter);
			return index < kCounterCount ? values[index] : 0;
		}

		// 自 prev 以来有没有「说明 MySQL 可能落后」的增长:produce_rejected、local_failure、
		// (delivery_failed − delivery_failed_superseded)、ticket_expired、exit_resave_capped、exit_unsynced、
		// orphan_unsynced 任一增长即为真。30s 汇总行据此升 WARN。
		bool AlarmingIncreaseSince(const Snapshot &prev) const
		{
			const auto grew = [&](Counter counter) { return Get(counter) > prev.Get(counter); };
			const auto unsuperseded = [](const Snapshot &snapshot) {
				const std::uint64_t failed = snapshot.Get(Counter::kDeliveryFailed);
				const std::uint64_t superseded = snapshot.Get(Counter::kDeliveryFailedSuperseded);
				return failed > superseded ? failed - superseded : 0;
			};
			return grew(Counter::kProduceRejected) || grew(Counter::kLocalFailure) ||
				   unsuperseded(*this) > unsuperseded(prev) || grew(Counter::kTicketExpired) ||
				   grew(Counter::kExitResaveCapped) || grew(Counter::kExitUnsynced) || grew(Counter::kOrphanUnsynced);
		}
	};

	Snapshot Read();
} // namespace db_task_dispatch_stats

class DbTaskDispatchSystem
{
public:
	// Pump 发现有玩家的 DBTask 落定(在途归零,或出现需要重存的失败)时逐个回调。只在 loop 顶层调用。
	using SettledHandler = std::function<void(Guid)>;

	struct SendResult
	{
		bool queued{false};
		std::string error; // 没入队时的原因(RdKafka::err2str),入队时为空
	};

	// 必须严格大于 librdkafka 的 message.timeout.ms(缺省 300000,本仓未覆盖;见 kafka_producer.cpp createProducer
	// 的注释):回执最晚在那之后到,超时兜底只兜「回执始终不来」。
	static constexpr std::chrono::seconds kTicketTimeout{330};
	// 停机时 ERROR 日志里列出的未落定玩家号上限。
	static constexpr std::size_t kShutdownUnsettledLogLimit = 50;

	// 登记回执监听器(本线程的 KafkaProducer)并保存 onSettled。RedisSystem::Initialize 调用。
	static void Initialize(SettledHandler onSettled);
	// 幂等:注销监听器,有未落定玩家时打一行 ERROR 列出(最多 kShutdownUnsettledLogLimit 个),清空 onSettled。
	static void Shutdown();

	static db_task_dispatch::SaveGeneration BeginSave(Guid playerId);
	// 领票 → sendTracked。没入队时销票、记失败、计 produce_rejected,返回 queued = false。
	static SendResult SendDbTask(Guid playerId, db_task_dispatch::SaveGeneration generation, const std::string &topic,
								 const std::string &payload, const std::string &key);
	// 本地失败(序列化失败等),同时计 local_failure。
	static void MarkLocalFailure(Guid playerId, db_task_dispatch::SaveGeneration generation);
	static void FinishSave(Guid playerId, db_task_dispatch::SaveGeneration generation, bool allQueued);

	static bool NeedsResave(Guid playerId);
	// 欠 MySQL 的那一代里出过永久失败(kafka_producer_policy::IsPermanentMessageError:消息过大、无写权限等,
	// 按当前内存重存也必然失败)时返回错误描述("<code> (<RdKafka::err2str>)",给日志用);否则返回空。
	static std::optional<std::string> PermanentFailure(Guid playerId);
	static std::uint32_t InFlight(Guid playerId);

	// 实体已离开本节点:没有在途票据就回收条目;回收时仍欠 MySQL 一次写则计 orphan_unsynced 并打 WARN。
	// 还有在途票据时不回收,等回执到了经 Pump → onSettled(实体已不在)再来一次。
	static void OnPlayerGone(Guid playerId);

	// 回执监听器的本体(也供单测直接驱动):只改账本、计数,需要处理的玩家号压进待办;不碰 ECS、不打逐条日志。
	// kafkaErrorCode 是回执里 RdKafka::ErrorCode 的原值(0 = ERR_NO_ERROR = acks=all 已确认),原样进账本:
	// 失败按 kafka_producer_policy::IsPermanentMessageError 分成可重试 / 永久两种(头文件不 include rdkafka,所以收整数)。
	static void OnDeliveryReport(db_task_dispatch::Ticket ticket, std::int32_t kafkaErrorCode);
	// 旧签名(bool delivered)已删除:true 会被静默转成错误码 1,把成功当失败记账。
	static void OnDeliveryReport(db_task_dispatch::Ticket ticket, bool delivered) = delete;

	// 50ms 定时器的本体,只能在 loop 顶层调用:poll() 派发回执 → 票据超时兜底 → 待办玩家排序去重后逐个交给 onSettled。
	static void Pump();

	static std::size_t UnsettledPlayerCount();
	static std::size_t InFlightTickets();

	static db_task_dispatch::Ledger &LedgerForTest();
	// 清账本与待办;不动监听器与 onSettled。
	static void ResetForTest();
};
