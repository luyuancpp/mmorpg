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

#include "proto/common/event/battle_event.pb.h"
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

// 冻结后的结果原件里，每名成员的 R07 个人结算字节(BattleSettlementEvent)，按 player_id 索引。
// 结果第一次发往 broker 之前用它写 pending，回执之后的派发也只用这同一份字节(审计 social-match-battle#2)。
// 回执之后不得再改写 pending：那时 scene 可能已应用并销账，账本条目随销账摘除；改写会让 pending 复活，
// 玩家下次登录就会再发一次奖。
// 原件不是 v1 完整来源、没有成员、成员缺 player_id 或重复、battle_id / outcome 与原件不符、序列化失败时，
// 返回 false 且不改 out。
inline bool BuildPersonalSettlementPayloads(const contracts::kafka::BattleResultEvent& committed,
                                            std::map<uint64_t, std::string>& out)
{
    if (committed.settlement_format_version() != 1 || committed.settlements_size() == 0) return false;
    std::map<uint64_t, std::string> payloads;
    for (const auto& settlement : committed.settlements())
    {
        if (settlement.player_id() == 0 || settlement.battle_id() != committed.battle_id() ||
            settlement.outcome() != committed.outcome()) return false;
        ::BattleSettlementEvent event;
        *event.mutable_settlement() = settlement;
        std::string payload;
        if (!event.SerializeToString(&payload) ||
            !payloads.emplace(settlement.player_id(), std::move(payload)).second) return false;
    }
    out = std::move(payloads);
    return true;
}

inline constexpr bool CreationReceiptExpired(uint64_t expiryMs,uint64_t nowMs,bool roomExists)
{
    return expiryMs < nowMs && !roomExists;
}
// 单逻辑线程所有。载荷一经计算不可改；只有本次关联号的 all ACK 能放行终局。
//
// 发送的前置条件(审计 social-match-battle#2)：载荷里全部成员的个人结算已写进 R07 pending。
// 在这之前 CanSend 恒为假。这样结果一旦可能到达 broker，battle 进程无论在回执前停机还是崩溃，
// 个人结算都已有耐久副本，由 scene 登录钩子 / 冻结到期补读应用，不依赖本进程活到回执。
// 落库按轮进行：BeginStaging 发号，调用方对全部成员各写一次，回复到齐后 FinishStaging 一次。
// 失败按与投递相同的有界退避重来；过期轮次(attempt 不符)的迟到回复不改状态。
class ResultCommit
{
public:
    using Clock = std::chrono::steady_clock;
    explicit ResultCommit(std::string payload) : payload_(std::move(payload)) {}
    const std::string& Payload() const { return payload_; }
    bool Confirmed() const { return confirmed_; }
    bool InFlight() const { return inFlight_ != 0; }
    uint32_t Failures() const { return failures_; }
    bool PersonalSettlementsDurable() const { return staged_; }
    // 开一轮落库，返回本轮编号；已落库、上一轮还在途或仍在退避期时返回 0。
    uint64_t BeginStaging(Clock::time_point now)
    {
        if (staged_ || stagingAttempt_ != 0 || now < stagingRetryAt_) return 0;
        stagingAttempt_ = ++stagingSeq_;
        return stagingAttempt_;
    }
    // 本轮全部成员的落库回复到齐后调用一次；返回 true 表示此后允许发送。
    bool FinishStaging(uint64_t attempt, bool allStored, Clock::time_point now)
    {
        if (attempt == 0 || attempt != stagingAttempt_ || staged_) return false;
        stagingAttempt_ = 0;
        if (allStored) { staged_ = true; return true; }
        stagingRetryAt_ = now + BackoffAfter(stagingFailures_);
        if (stagingFailures_ != UINT32_MAX) ++stagingFailures_;
        return false;
    }
    bool CanSend(Clock::time_point now) const { return staged_ && !confirmed_ && !InFlight() && now >= retryAt_; }
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
        const auto delay = BackoffAfter(failures_);
        if (failures_ != UINT32_MAX) ++failures_;
        retryAt_ = now + delay;
        return false;
    }
private:
    static std::chrono::seconds BackoffAfter(uint32_t failures)
    {
        return std::chrono::seconds(std::min(1u << std::min(failures, uint32_t{5}), uint32_t{30}));
    }

    std::string payload_;
    uint64_t inFlight_ = 0;
    uint32_t failures_ = 0;
    bool confirmed_ = false;
    Clock::time_point retryAt_{};
    bool staged_ = false;
    uint64_t stagingAttempt_ = 0;
    uint64_t stagingSeq_ = 0;
    uint32_t stagingFailures_ = 0;
    Clock::time_point stagingRetryAt_{};
};
}
