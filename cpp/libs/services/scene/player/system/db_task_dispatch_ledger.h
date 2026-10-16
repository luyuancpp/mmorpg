#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <unordered_map>
#include <vector>

// 玩家存盘 DBTask 的投递账本(纯数据结构,不 include ECS / rdkafka,不读墙钟 —— 时刻一律由调用方传入)。
// 粘合层是 db_task_dispatch_system.h(DbTaskDispatchSystem),单测在 cpp/tests/cross_zone_test/db_task_dispatch_test.cpp。
//
// 要解决的缺陷:scene 存盘时 Redis 与 DBTask(Kafka → go/db → MySQL)是两条独立的写。旧实现里 DBTask 同步被拒或
// 投递回执失败只打一行日志,Redis 落地回调照样把 dirty-save 快照(PlayerLastPersistedSnapshotComp)前移,
// 下一次存盘被「盘上已是同一份」的快路径跳过 —— MySQL 永久落后于 Redis。快照本身的语义是「字节确实已进 Redis」
// (战斗结算 / 资产通道拿它销账),不能为 Kafka 去作废它;这里另记一本账,告诉快路径「这个玩家还欠 MySQL 一次整份写」。
//
// 记账单位:
//   代号(SaveGeneration)  每次存盘 BeginSave 领一个,全局单调递增(只在同一玩家内比较大小);
//   票据(Ticket)          每条 DBTask 一张,IssueTicket 领,作为 KafkaProducer::sendTracked 的关联号,全局单调递增、不复用。
// 每个玩家的状态:
//   inFlight          已发出、还没有结局(回执 / 同步拒绝 / 超时)的票据数;
//   lastFullyQueued   最近一次「全部 DBTask 都同步入队」的存盘代号;
//   maxFailed         出过失败(同步拒绝、本地序列化失败、回执失败、超时)的最大代号。
// NeedsResave = maxFailed != 0 && maxFailed >= lastFullyQueued:有一次失败还没被一次更新的、整份入队的存盘盖过。
// 用代号比大小而不是布尔清零:回执可能先于 FinishSave 到(inFlight 在真正 produce 之前就加了,回执可能在 send()
// 内部的 poll 里就派发),也可能晚于下一次存盘才到;两种顺序下结论都一样,不需要额外的时序约定。
// 一次失败之后,只有之后某次存盘的 DBTask **全部**同步入队、且没有代号 ≥ 它的失败回执,NeedsResave 才自然解除;
// 那次存盘自己的回执再失败,又会重新置位。
//
// 线程模型:单个 loop 线程独占(scene 主循环),不加锁。
namespace db_task_dispatch
{
	using PlayerId = std::uint64_t;
	using SaveGeneration = std::uint64_t;
	using Ticket = std::uint64_t;
	using Clock = std::chrono::steady_clock;

	// 0 不是合法票据:KafkaProducer 把关联号 0 当作「不跟踪」。
	inline constexpr Ticket kNoTicket = 0;

	// 一张票据有了结局之后对账本的影响。底层类型数的是结局种类数(AGENTS §11.2)。
	enum class DeliveryEffect : std::uint8_t
	{
		kUnknownTicket,     // 票据不认识(重复回执、超时之后的迟到回执、玩家条目已回收):什么都没改
		kDelivered,         // 投递成功(acks=all)
		kFailedSuperseded,  // 投递失败,但已有一次更新的整份入队存盘盖过它
		kFailedNeedsResave, // 投递失败,且它属于最近一次整份入队的存盘(或更新):需要重存

		kCount
	};

	inline constexpr std::size_t kDeliveryEffectCount = static_cast<std::size_t>(DeliveryEffect::kCount);

	inline constexpr const char *kDeliveryEffectNames[] = {
		"unknown_ticket",
		"delivered",
		"failed_superseded",
		"failed_needs_resave",
	};
	static_assert(std::size(kDeliveryEffectNames) == kDeliveryEffectCount, "kDeliveryEffectNames 必须与 DeliveryEffect 一一对应");

	constexpr const char *DeliveryEffectName(DeliveryEffect effect)
	{
		const auto index = static_cast<std::size_t>(effect);
		return index < kDeliveryEffectCount ? kDeliveryEffectNames[index] : "?";
	}

	struct DeliveryOutcome
	{
		PlayerId playerId{0};
		DeliveryEffect effect{DeliveryEffect::kUnknownTicket};
		std::uint32_t inFlightAfter{0};
		bool needsResaveAfter{false};
	};

	struct ForgetResult
	{
		bool forgotten{false};
		bool hadUnresolvedFailure{false};
	};

	// dirty-save 快路径能不能跳过本次存盘:调用方允许跳过、快照与当前内存相等、且没有欠 MySQL 的失败。
	// 有欠账时强制真写:重新 marshal 当前内存整份重发,绝不重放旧载荷(go/db 的游标与 owner_epoch 都挡不住
	// 同 epoch、更大 offset 的旧字节)。
	constexpr bool AllowsDirtySaveSkip(bool allowSkipWhenPersisted, bool snapshotMatches, bool needsResave)
	{
		return allowSkipWhenPersisted && snapshotMatches && !needsResave;
	}

	class Ledger
	{
	public:
		// 开始一次存盘:按需建玩家条目,返回本次的代号(单调递增,从 1 起)。
		SaveGeneration BeginSave(PlayerId playerId);

		// 登记一条即将发送的 DBTask:inFlight + 1,返回票据。**必须在真正 produce 之前调用**:回执可能在
		// send() 内部的 poll 里就到,那时票据必须已经在账上。
		Ticket IssueTicket(PlayerId playerId, SaveGeneration generation, Clock::time_point now);

		// 同步发送被拒(没有入队,永远不会有回执):inFlight - 1、记失败、销票。票据不认识时什么都不做。
		void OnProduceRejected(Ticket ticket);

		// 本地失败(序列化失败等,根本没走到发送):记失败。
		void MarkLocalFailure(PlayerId playerId, SaveGeneration generation);

		// 本次存盘的 DBTask 都发完了。allQueued = 每一条都同步入队;否则这一代也记为失败(调用方通常已经经
		// OnProduceRejected / MarkLocalFailure 记过,这里再记一次是防御,不改变结论)。
		void FinishSave(PlayerId playerId, SaveGeneration generation, bool allQueued);

		// 票据有了结局(librdkafka 回执、重建时的合成失败、超时兜底)。票据不认识,或玩家条目已被回收,
		// 返回 kUnknownTicket(后者同样销票)。
		DeliveryOutcome OnDelivery(Ticket ticket, bool delivered);

		// 超时兜底:按发票顺序检查,发出已满 timeout 仍没有结局的票据按投递失败处理。已经有结局的票据直接跳过;
		// 碰到第一张还没到期的活票据就停(发票顺序 = 时间顺序)。
		std::vector<DeliveryOutcome> ExpireStale(Clock::time_point now, Clock::duration timeout);

		bool NeedsResave(PlayerId playerId) const;
		std::uint32_t InFlight(PlayerId playerId) const;
		// 没有在途票据、也不欠重存。没有条目视为已落定。
		bool IsSettled(PlayerId playerId) const;

		// 玩家离开本节点(实体销毁)时回收条目:没有条目 → {false, false};还有在途票据 → 不回收,
		// {false, NeedsResave};否则回收,{true, 回收前的 NeedsResave}。
		ForgetResult ForgetIfNoInFlight(PlayerId playerId);

		std::size_t TrackedPlayers() const;
		std::size_t OutstandingTickets() const;
		std::size_t UnsettledPlayerCount() const;
		// 未落定的玩家号,升序,最多 limit 个(停机日志用)。
		std::vector<PlayerId> UnsettledPlayers(std::size_t limit) const;

		// 清空全部条目与票据(单测用)。代号与票据计数器**不归零**:生产者的关联号登记簿里可能还挂着旧票据号,
		// 复用会让迟到的旧回执记到新票据头上。
		void Clear();

	private:
		struct PlayerState
		{
			std::uint32_t inFlight{0};
			SaveGeneration lastFullyQueued{0};
			SaveGeneration maxFailed{0};
		};

		struct TicketInfo
		{
			PlayerId playerId{0};
			SaveGeneration generation{0};
			Clock::time_point issuedAt{};
		};

		static bool StateNeedsResave(const PlayerState &state);
		static bool StateIsSettled(const PlayerState &state);
		// inFlight - 1;已经是 0 时不下溢,打 ERROR 并钳在 0(说明账本与票据表不一致)。
		static void DecrementInFlight(PlayerState &state, PlayerId playerId);

		std::unordered_map<PlayerId, PlayerState> players_;
		std::unordered_map<Ticket, TicketInfo> tickets_;
		// 发票顺序,只给 ExpireStale 用。已有结局的票据留在这里,ExpireStale 从队头顺手弹掉。
		std::deque<Ticket> issueOrder_;
		SaveGeneration nextGeneration_{1};
		Ticket nextTicket_{1};
	};
} // namespace db_task_dispatch
