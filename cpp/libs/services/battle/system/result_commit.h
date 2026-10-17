#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "proto/contracts/kafka/match_event.pb.h"

namespace turnbattle
{
// 小于Kafka默认单消息约1MB，给键和协议开销留空间；两版消费者同一上限。
inline constexpr std::size_t kMaxCompleteResultBytes = 960 * 1024;

// 从首次引擎结算固定完整来源。先校验全部成员，失败不修改event；强制DRAW统一盖章。
// 仅归档来源，不能以这个格式版本代替玩家归属、源存档版本或资产SQL收据。
inline bool FreezeBattleSettlementSource(contracts::kafka::BattleResultEvent& event,
                                        std::vector<::BattleSettlementData> settlements)
{
    if (event.battle_id() == 0 || !::eBattleOutcome_IsValid(event.outcome()) ||
        event.outcome() == ::BATTLE_OUTCOME_ONGOING || settlements.empty() ||
        event.settlement_format_version() != 0 || event.settlements_size() != 0)
        return false;
    std::map<uint64_t, uint32_t> members;
    std::set<uint32_t> teams;
    for (const auto& team : event.teams())
    {
        if (team.player_ids().empty() || !teams.insert(team.team_index()).second) return false;
        for (const auto playerId : team.player_ids())
            if (playerId == 0 || !members.emplace(playerId, team.team_index()).second) return false;
    }
    if (members.size() != settlements.size()) return false;
    std::sort(settlements.begin(), settlements.end(), [](const auto& a, const auto& b)
    { return a.player_id() < b.player_id(); });
    uint64_t previous = 0;
    for (const auto& settlement : settlements)
    {
        const auto member = members.find(settlement.player_id());
        if (member == members.end() || settlement.player_id() == previous ||
            member->second != settlement.player_team_index() ||
            settlement.total_rounds() != event.total_rounds() ||
            (settlement.battle_id() != 0 && settlement.battle_id() != event.battle_id())) return false;
        previous = settlement.player_id();
    }
    auto frozen = event;
    for (auto& settlement : settlements)
    {
        settlement.set_battle_id(event.battle_id());
        settlement.set_outcome(event.outcome());
        *frozen.add_settlements() = std::move(settlement);
    }
    frozen.set_settlement_format_version(1);
    if (frozen.ByteSizeLong() > kMaxCompleteResultBytes) return false;
    event = std::move(frozen);
    return true;
}

inline constexpr bool CreationReceiptExpired(uint64_t expiryMs,uint64_t nowMs,bool roomExists)
{
    return expiryMs < nowMs && !roomExists;
}
// 单逻辑线程所有。载荷一经计算不可改；只有本次关联号的 all ACK 能放行终局。
class ResultCommit
{
public:
    using Clock = std::chrono::steady_clock;
    explicit ResultCommit(std::string payload) : payload_(std::move(payload)) {}
    const std::string& Payload() const { return payload_; }
    bool Confirmed() const { return confirmed_; }
    bool InFlight() const { return inFlight_ != 0; }
    uint32_t Failures() const { return failures_; }
    bool CanSend(Clock::time_point now) const { return !confirmed_ && !InFlight() && now >= retryAt_; }
    bool Begin(uint64_t correlation, Clock::time_point now)
    {
        if (correlation == 0 || !CanSend(now)) return false;
        inFlight_ = correlation;
        return true;
    }
    bool Deliver(uint64_t correlation, bool allAcknowledged, Clock::time_point now)
    {
        if (correlation == 0 || correlation != inFlight_ || confirmed_) return false;
        inFlight_ = 0;
        if (allAcknowledged) { confirmed_ = true; return true; }
        const auto delay = std::chrono::seconds(std::min(1u << std::min(failures_, uint32_t{5}), uint32_t{30}));
        if (failures_ != UINT32_MAX) ++failures_;
        retryAt_ = now + delay;
        return false;
    }
private:
    std::string payload_;
    uint64_t inFlight_ = 0;
    uint32_t failures_ = 0;
    bool confirmed_ = false;
    Clock::time_point retryAt_{};
};
}
