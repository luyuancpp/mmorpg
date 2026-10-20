#include "db_task_dispatch_ledger.h"

#include <algorithm>

#include "muduo/base/Logging.h"

namespace db_task_dispatch
{
	bool Ledger::StateNeedsResave(const PlayerState &state)
	{
		return state.maxFailed != 0 && state.maxFailed >= state.lastFullyQueued;
	}

	bool Ledger::StateIsSettled(const PlayerState &state)
	{
		return state.inFlight == 0 && !StateNeedsResave(state);
	}

	void Ledger::RecordFailure(PlayerState &state, SaveGeneration generation, Failure failure)
	{
		state.maxFailed = std::max(state.maxFailed, generation);
		// 同一代先后出了多次永久失败(两张子表都超限)时记最后一次的错误码;更旧的一代来的永久失败不改结论。
		if (failure.kind == FailureKind::kPermanent && generation >= state.maxPermanentFailed)
		{
			state.maxPermanentFailed = generation;
			state.permanentFailureCode = failure.code;
		}
	}

	void Ledger::DecrementInFlight(PlayerState &state, PlayerId playerId)
	{
		if (state.inFlight == 0)
		{
			// 票据表里还有这张票,玩家条目的在途数却已是 0:账本内部不一致。钳在 0,不让它回绕成 4G 张在途票据
			// (那会让该玩家的退出永远等下去)。
			LOG_ERROR << "[DbTaskDispatch] in-flight counter underflow for player " << playerId
					  << "; clamping to 0 (ledger and ticket table disagree)";
			return;
		}
		--state.inFlight;
	}

	SaveGeneration Ledger::BeginSave(PlayerId playerId)
	{
		players_.try_emplace(playerId);
		return nextGeneration_++;
	}

	Ticket Ledger::IssueTicket(PlayerId playerId, SaveGeneration generation, Clock::time_point now)
	{
		auto &state = players_[playerId]; // 按需建条目:正常流程里 BeginSave 已经建过
		++state.inFlight;
		const Ticket ticket = nextTicket_++;
		tickets_.insert_or_assign(ticket, TicketInfo{playerId, generation, now});
		issueOrder_.push_back(ticket);
		return ticket;
	}

	void Ledger::OnProduceRejected(Ticket ticket, Failure failure)
	{
		const auto ticketIt = tickets_.find(ticket);
		if (ticketIt == tickets_.end())
		{
			return;
		}
		const TicketInfo info = ticketIt->second;
		tickets_.erase(ticketIt);
		const auto playerIt = players_.find(info.playerId);
		if (playerIt == players_.end())
		{
			return;
		}
		DecrementInFlight(playerIt->second, info.playerId);
		RecordFailure(playerIt->second, info.generation, failure);
	}

	void Ledger::MarkLocalFailure(PlayerId playerId, SaveGeneration generation, Failure failure)
	{
		auto &state = players_[playerId]; // 失败必须记下来,条目不在也要建
		RecordFailure(state, generation, failure);
	}

	void Ledger::FinishSave(PlayerId playerId, SaveGeneration generation, bool allQueued)
	{
		auto &state = players_[playerId];
		if (allQueued)
		{
			state.lastFullyQueued = std::max(state.lastFullyQueued, generation);
			return;
		}
		state.maxFailed = std::max(state.maxFailed, generation);
	}

	DeliveryOutcome Ledger::OnDelivery(Ticket ticket, bool delivered, Failure failure)
	{
		DeliveryOutcome outcome;
		const auto ticketIt = tickets_.find(ticket);
		if (ticketIt == tickets_.end())
		{
			return outcome; // 重复回执 / 超时之后的迟到回执 / 从未登记
		}
		const TicketInfo info = ticketIt->second;
		tickets_.erase(ticketIt);
		outcome.playerId = info.playerId;

		const auto playerIt = players_.find(info.playerId);
		if (playerIt == players_.end())
		{
			return outcome; // 玩家条目已被回收:票已销,结局不再影响任何人
		}
		auto &state = playerIt->second;
		DecrementInFlight(state, info.playerId);
		if (delivered)
		{
			outcome.effect = DeliveryEffect::kDelivered;
		}
		else
		{
			RecordFailure(state, info.generation, failure);
			outcome.effect = info.generation >= state.lastFullyQueued ? DeliveryEffect::kFailedNeedsResave
																	  : DeliveryEffect::kFailedSuperseded;
		}
		outcome.inFlightAfter = state.inFlight;
		outcome.needsResaveAfter = StateNeedsResave(state);
		return outcome;
	}

	std::vector<DeliveryOutcome> Ledger::ExpireStale(Clock::time_point now, Clock::duration timeout)
	{
		std::vector<DeliveryOutcome> expired;
		while (!issueOrder_.empty())
		{
			const Ticket ticket = issueOrder_.front();
			const auto ticketIt = tickets_.find(ticket);
			if (ticketIt == tickets_.end())
			{
				issueOrder_.pop_front(); // 已有结局
				continue;
			}
			if (now - ticketIt->second.issuedAt < timeout)
			{
				break; // 发票顺序 = 时间顺序:后面的更新,不用再看
			}
			issueOrder_.pop_front();
			expired.push_back(OnDelivery(ticket, /*delivered=*/false));
		}
		return expired;
	}

	bool Ledger::NeedsResave(PlayerId playerId) const
	{
		const auto playerIt = players_.find(playerId);
		return playerIt != players_.end() && StateNeedsResave(playerIt->second);
	}

	std::optional<std::int32_t> Ledger::PermanentFailureCode(PlayerId playerId) const
	{
		const auto playerIt = players_.find(playerId);
		if (playerIt == players_.end())
		{
			return std::nullopt;
		}
		const PlayerState &state = playerIt->second;
		// 只看欠账的那一代:更新的一代只出了可重试的失败(maxPermanentFailed < maxFailed),重存仍可能成功。
		if (!StateNeedsResave(state) || state.maxPermanentFailed == 0 || state.maxPermanentFailed != state.maxFailed)
		{
			return std::nullopt;
		}
		return state.permanentFailureCode;
	}

	std::uint32_t Ledger::InFlight(PlayerId playerId) const
	{
		const auto playerIt = players_.find(playerId);
		return playerIt != players_.end() ? playerIt->second.inFlight : 0;
	}

	bool Ledger::IsSettled(PlayerId playerId) const
	{
		const auto playerIt = players_.find(playerId);
		return playerIt == players_.end() || StateIsSettled(playerIt->second);
	}

	ForgetResult Ledger::ForgetIfNoInFlight(PlayerId playerId)
	{
		ForgetResult result;
		const auto playerIt = players_.find(playerId);
		if (playerIt == players_.end())
		{
			return result;
		}
		result.hadUnresolvedFailure = StateNeedsResave(playerIt->second);
		if (playerIt->second.inFlight > 0)
		{
			return result;
		}
		players_.erase(playerIt);
		result.forgotten = true;
		return result;
	}

	std::size_t Ledger::TrackedPlayers() const
	{
		return players_.size();
	}

	std::size_t Ledger::OutstandingTickets() const
	{
		return tickets_.size();
	}

	std::size_t Ledger::UnsettledPlayerCount() const
	{
		return static_cast<std::size_t>(std::count_if(players_.begin(), players_.end(),
													  [](const auto &entry) { return !StateIsSettled(entry.second); }));
	}

	std::vector<PlayerId> Ledger::UnsettledPlayers(std::size_t limit) const
	{
		std::vector<PlayerId> unsettled;
		for (const auto &[playerId, state] : players_)
		{
			if (!StateIsSettled(state))
			{
				unsettled.push_back(playerId);
			}
		}
		std::sort(unsettled.begin(), unsettled.end());
		if (unsettled.size() > limit)
		{
			unsettled.resize(limit);
		}
		return unsettled;
	}

	void Ledger::Clear()
	{
		players_.clear();
		tickets_.clear();
		issueOrder_.clear();
	}
} // namespace db_task_dispatch
