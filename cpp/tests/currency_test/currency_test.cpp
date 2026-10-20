#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"

#include "infra/storage/redis_client/redis_client.h"
#include "modules/currency/constants/currency.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/currency/system/currency_system.h"
#include "modules/transaction_log/anomaly_detector.h"
#include "player/comp/player_frozen_comp.h" // 冻结用例:IsCrossZoneFrozen 就是 any_of<PlayerFrozenComp>
#include "player/comp/player_persist_writer_comp.h" // 严格屏障写方令牌(混版本持久字段保护 §5.2)
#include "engine/config/persist_schema_regression_mode.h"
#include "player/system/handoff_sql_barrier.h"
#include "proto/common/component/currency_comp.pb.h"
#include "table/proto/tip/asset_error_tip.pb.h" // kAssetFrozen / kAssetBlocked / kAssetCurrencyInsufficient
#include "table/proto/tip/common_error_tip.pb.h"
#include <thread_context/ecs_context.h>
#include "modules/id_segment/guid_segment_registry.h"
#include "../test_config_helper.h"

template <class MessageKey, class MessageValue>
struct MessageAsyncClientTestPeer
{
    using Client = MessageAsyncClient<MessageKey, MessageValue>;
    using Element = typename Client::Element;
    using ElementPtr = typename Client::ElementPtr;

    static ElementPtr TrackLoad(Client& client, const MessageKey& key)
    {
        auto element = std::make_shared<Element>();
        element->message_key = key;
        element->redis_key = client.full_name() + ":" + std::to_string(key);
        client.loading_queue_[element->redis_key] = element;
        return element;
    }

    static void Deliver(Client& client, redisReply* reply, const ElementPtr& element)
    {
        client.OnLoaded(nullptr, reply, element);
    }

    // 该 key 当前在载入重试队列里的那份;没有则返回 nullptr。
    static ElementPtr PendingLoad(Client& client, const MessageKey& key)
    {
        const auto it = client.pending_retry_queue_.find(client.full_name() + ":" + std::to_string(key));
        return it == client.pending_retry_queue_.end() ? nullptr : it->second;
    }

    // 模拟一条带 guard 的存盘已经发出、正在等回包(不连 Redis,直接登记进 saving_queue_)。
    static ElementPtr TrackGuardedSave(Client& client, const MessageKey& key,
                                       const std::string& guardKey, const std::string& guardExpected)
    {
        auto element = std::make_shared<Element>();
        element->message_key = key;
        element->redis_key = client.full_name() + ":" + std::to_string(key);
        element->message_value = std::make_shared<MessageValue>();
        element->guard_key = guardKey;
        element->guard_expected = guardExpected;
        client.saving_queue_[element->redis_key] = element;
        return element;
    }

    static void DeliverSaved(Client& client, redisReply* reply, const ElementPtr& element)
    {
        client.OnSaved(nullptr, reply, element);
    }

    // 模拟一条普通(无 guard)存盘已经发出、正在等回包;载荷由调用方给,用来区分新旧值。
    static ElementPtr TrackSave(Client& client, const MessageKey& key, const std::shared_ptr<MessageValue>& value)
    {
        auto element = std::make_shared<Element>();
        element->message_key = key;
        element->redis_key = client.full_name() + ":" + std::to_string(key);
        element->message_value = value;
        client.saving_queue_[element->redis_key] = element;
        return element;
    }

    // 该 key 当前排队中的那份;没有则返回 nullptr。
    static ElementPtr PendingSave(Client& client, const MessageKey& key)
    {
        const auto it = client.pending_save_queue_.find(client.full_name() + ":" + std::to_string(key));
        return it == client.pending_save_queue_.end() ? nullptr : it->second;
    }

    // 模拟定时器到期把排队值发出:挪进在途,不碰 hiredis(没有连接时 IssueSave 只会把它放回排队)。
    // 调用方要保证此刻没有在途 —— 与 IssueSave 的前提相同。
    static ElementPtr IssuePendingForTest(Client& client, const MessageKey& key)
    {
        const std::string redisKey = client.full_name() + ":" + std::to_string(key);
        const auto it = client.pending_save_queue_.find(redisKey);
        if (it == client.pending_save_queue_.end() || client.saving_queue_.count(redisKey) != 0)
        {
            return nullptr;
        }
        auto element = it->second;
        client.pending_save_queue_.erase(it);
        client.saving_queue_[redisKey] = element;
        return element;
    }

    // 三个 ScriptSlot 里实际会经 EVALSHA / EVAL 发出的脚本文本。
    // 断言打在发出的文本上而不是常量名上:槽位换绑到别的常量也逃不过。
    static std::vector<std::string_view> SaveScriptSources(const Client& client)
    {
        return {client.save_script_.source, client.guard_script_.source, client.barrier_script_.source};
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

entt::entity CreateTestPlayer()
{
    const auto player = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.emplace<CurrencyComp>(player);
    tlsEcs.actorRegistry.emplace<PlayerCurrencyComp>(player);
    return player;
}

void DestroyTestPlayer(entt::entity player)
{
    tlsEcs.actorRegistry.destroy(player);
}

// ---------------------------------------------------------------------------
// AddCurrency
// ---------------------------------------------------------------------------

TEST(CurrencyTest, AddCurrencyBasic)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));

    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 50));
    EXPECT_EQ(150u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AddCurrencyInvalidAmount)
{
    auto player = CreateTestPlayer();

    EXPECT_NE(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 0));
    EXPECT_NE(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, -1));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AddCurrencyMultipleTypes)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyDiamond, 50));
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyBindDiamond, 30));

    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(50u, CurrencySystem::GetBalance(player, kCurrencyDiamond));
    EXPECT_EQ(30u, CurrencySystem::GetBalance(player, kCurrencyBindDiamond));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// DeductCurrency
// ---------------------------------------------------------------------------

TEST(CurrencyTest, DeductCurrencyBasic)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 200);
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 80));
    EXPECT_EQ(120u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DeductCurrencyInsufficientFunds)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 50);
    EXPECT_NE(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 100));
    // Balance stays unchanged after failed deduction.
    EXPECT_EQ(50u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DeductCurrencyInvalidAmount)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 100);
    EXPECT_NE(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 0));
    EXPECT_NE(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, -5));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DeductCurrencyExact)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyDiamond, 77);
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyDiamond, 77));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyDiamond));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// CanAfford
// ---------------------------------------------------------------------------

TEST(CurrencyTest, CanAfford)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 100);
    EXPECT_TRUE(CurrencySystem::CanAfford(player, kCurrencyGold, 50));
    EXPECT_TRUE(CurrencySystem::CanAfford(player, kCurrencyGold, 100));
    EXPECT_FALSE(CurrencySystem::CanAfford(player, kCurrencyGold, 101));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// AttachDebt (补缴)
// ---------------------------------------------------------------------------

TEST(CurrencyTest, DebtDeductsFromGain)
{
    auto player = CreateTestPlayer();

    // Attach a debt of 60 gold.
    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyGold, 60));

    // Add 100 gold — 60 goes to debt, 40 is credited.
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(40u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DebtPartialRepayment)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyGold, 100));

    // First gain: 30 gold — all goes to debt.
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 30));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    // Second gain: 50 gold — all goes to debt (remaining debt was 70).
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 50));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    // Third gain: 30 gold — 20 goes to debt, 10 credited.
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 30));
    EXPECT_EQ(10u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DebtDoesNotAffectOtherTypes)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyGold, 100));

    // Diamond has no debt — full credit.
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyDiamond, 50));
    EXPECT_EQ(50u, CurrencySystem::GetBalance(player, kCurrencyDiamond));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AdjustDebtClampsOverDecreaseAndInt64Min)
{
    auto player = CreateTestPlayer();
    auto &comp = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player);
    auto &debt = comp.debts[static_cast<uint32_t>(kCurrencyGold)];
    debt.owed = 100;
    debt.paid = 40;

    CurrencySystem::AdjustDebt(player, kCurrencyGold, -200);
    EXPECT_EQ(40u, debt.owed);
    EXPECT_EQ(40u, debt.paid);

    debt.owed = 100;
    CurrencySystem::AdjustDebt(player, kCurrencyGold, std::numeric_limits<int64_t>::min());
    EXPECT_EQ(40u, debt.owed);

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DebtIncreaseRejectsUint64Overflow)
{
    auto player = CreateTestPlayer();
    auto &comp = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player);
    auto &debt = comp.debts[static_cast<uint32_t>(kCurrencyGold)];
    debt.owed = std::numeric_limits<uint64_t>::max() - 5;

    EXPECT_EQ(kInvalidParameter, CurrencySystem::AttachDebt(player, kCurrencyGold, 10))
        << "溢出拒绝必须如实回码,收件箱执行器据此记 REJECTED";
    EXPECT_EQ(std::numeric_limits<uint64_t>::max() - 5, debt.owed);

    CurrencySystem::AdjustDebt(player, kCurrencyGold, 10);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max() - 5, debt.owed);

    DestroyTestPlayer(player);
}

// AttachDebt 改为返回 tip 码(docs/design/player-inbox-service.md §7.6):纯参数错误与缺组件都回
// kInvalidParameter,且不留下任何欠款条目。
TEST(CurrencyTest, AttachDebtReportsInvalidParametersWithoutSideEffects)
{
    auto player = CreateTestPlayer();
    const auto &comp = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player);

    EXPECT_EQ(kInvalidParameter, CurrencySystem::AttachDebt(player, kCurrencyGold, 0));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AttachDebt(player, kCurrencyGold, -5));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AttachDebt(player, kCurrencyMax, 10)) << "越界币种";
    EXPECT_TRUE(comp.debts.empty()) << "拒绝路径不得落下欠款条目";

    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyGold, 7, "audit", "gm"));
    ASSERT_EQ(1u, comp.debts.count(static_cast<uint32_t>(kCurrencyGold)));
    EXPECT_EQ(7u, comp.debts.at(static_cast<uint32_t>(kCurrencyGold)).owed);

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AttachDebtWithoutRuntimeCurrencyCompIsRejected)
{
    auto player = CreateTestPlayer();
    tlsEcs.actorRegistry.remove<PlayerCurrencyComp>(player);

    EXPECT_EQ(kInvalidParameter, CurrencySystem::AttachDebt(player, kCurrencyGold, 10));
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerCurrencyComp>(player)) << "只取不建";

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DebtPersistenceRoundTrip)
{
    PlayerCurrencyComp source;
    CurrencyDebt debt;
    debt.owed = 500;
    debt.paid = 123;
    debt.frozen = true;
    debt.expiresAt = 9999;
    debt.reason = "audit";
    debt.gmOperator = "gm";
    debt.createdAt = 777;
    source.debts[static_cast<uint32_t>(kCurrencyGold)] = debt;

    CurrencyComp proto;
    ASSERT_TRUE(source.SaveToProto(proto));

    PlayerCurrencyComp restored;
    restored.LoadFromProto(proto);
    const auto it = restored.debts.find(static_cast<uint32_t>(kCurrencyGold));
    ASSERT_NE(restored.debts.end(), it);
    EXPECT_EQ(debt.owed, it->second.owed);
    EXPECT_EQ(debt.paid, it->second.paid);
    EXPECT_EQ(debt.frozen, it->second.frozen);
    EXPECT_EQ(debt.expiresAt, it->second.expiresAt);
    EXPECT_EQ(debt.reason, it->second.reason);
    EXPECT_EQ(debt.gmOperator, it->second.gmOperator);
    EXPECT_EQ(debt.createdAt, it->second.createdAt);
}

// 欠款上本版本不认识的字段随这条欠款携带、存盘原样写回;欠款删除后重建得到新对象,不带旧欠款的新字段
// (docs/design/mixed-version-persist-field-protection.md §4.1 的复活反例,§10 的回归用例)。
TEST(CurrencyTest, DebtUnknownFieldsTravelWithTheDebtOnly)
{
    constexpr int kFutureField = 9999; // 本版本不认识的字段号
    CurrencyComp persisted;
    auto *entry = persisted.add_debts();
    entry->set_currency_type(static_cast<uint32_t>(kCurrencyGold));
    entry->set_owed(500);
    entry->mutable_unknown_fields()->AddVarint(kFutureField, 42);

    auto player = CreateTestPlayer();
    auto &runtime = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player);
    runtime.LoadFromProto(persisted);

    CurrencyComp saved;
    ASSERT_TRUE(runtime.SaveToProto(saved));
    ASSERT_EQ(1, saved.debts_size());
    ASSERT_EQ(1, saved.debts(0).unknown_fields().field_count()) << "未知字段必须随欠款写回";
    EXPECT_EQ(kFutureField, saved.debts(0).unknown_fields().field(0).number());
    EXPECT_EQ(42u, saved.debts(0).unknown_fields().field(0).varint());

    // 还清删除后再挂同币种:新欠款是新对象,不得带上旧欠款的未知字段。
    runtime.debts.erase(static_cast<uint32_t>(kCurrencyGold));
    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyGold, 80));
    CurrencyComp reattached;
    ASSERT_TRUE(runtime.SaveToProto(reattached));
    ASSERT_EQ(1, reattached.debts_size());
    EXPECT_EQ(80u, reattached.debts(0).owed());
    EXPECT_TRUE(reattached.debts(0).unknown_fields().empty()) << "旧欠款的新字段复活到了新欠款上";

    DestroyTestPlayer(player);
}

// 已有欠款带着本版本不认识的字段时,旧版本不把新的一笔并进去(设计 §4.4)。
TEST(CurrencyTest, AttachDebtRefusesToMergeIntoDebtWithUnknownFields)
{
    CurrencyComp persisted;
    auto *entry = persisted.add_debts();
    entry->set_currency_type(static_cast<uint32_t>(kCurrencyGold));
    entry->set_owed(500);
    entry->mutable_unknown_fields()->AddVarint(9999, 1);

    auto player = CreateTestPlayer();
    auto &runtime = tlsEcs.actorRegistry.get<PlayerCurrencyComp>(player);
    runtime.LoadFromProto(persisted);

    // 只读谓词与原语同口径:调用方(收件箱执行器)靠它先判、回延后,不拿通用码 kFeatureUnavailable 反推。
    EXPECT_TRUE(CurrencySystem::DebtCarriesUnknownFields(player, kCurrencyGold));
    EXPECT_FALSE(CurrencySystem::DebtCarriesUnknownFields(player, kCurrencyDiamond)) << "没有这种币种的欠款";
    EXPECT_FALSE(CurrencySystem::DebtCarriesUnknownFields(player, kCurrencyMax)) << "越界币种";

    EXPECT_EQ(kFeatureUnavailable, CurrencySystem::AttachDebt(player, kCurrencyGold, 100));
    EXPECT_EQ(500u, runtime.debts.at(static_cast<uint32_t>(kCurrencyGold)).owed) << "拒绝时欠款不得改动";
    // 别的币种没有携带未知字段,照常可挂。
    EXPECT_EQ(kSuccess, CurrencySystem::AttachDebt(player, kCurrencyDiamond, 100));
    EXPECT_FALSE(CurrencySystem::DebtCarriesUnknownFields(player, kCurrencyDiamond)) << "新建的欠款不带未知字段";

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AnomalyBucketsSeparateCurrencyAndItemAndClearForTestIsolation)
{
    AnomalyDetector::ClearAll();
    const uint32_t sharedNumericKey = static_cast<uint32_t>(kCurrencyGold);

    AnomalyThreshold currencyThreshold;
    currencyThreshold.maxCountPerWindow = 100;
    currencyThreshold.maxAmountPerWindow = std::numeric_limits<uint32_t>::max();
    currencyThreshold.windowSeconds = 600;
    AnomalyDetector::SetCurrencyThreshold(kCurrencyGold, currencyThreshold);

    AnomalyThreshold itemThreshold = currencyThreshold;
    itemThreshold.maxCountPerWindow = 1;
    AnomalyDetector::SetItemThreshold(sharedNumericKey, itemThreshold);

    auto player = CreateTestPlayer();
    EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(player, kCurrencyGold, 1));
    // A numeric collision with CurrencyType must not make this first item event
    // look like the second event in the same bucket.
    EXPECT_FALSE(AnomalyDetector::RecordItemGain(player, sharedNumericKey, 1));
    EXPECT_EQ(1u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));

    AnomalyDetector::ClearAll();
    EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));

    DestroyTestPlayer(player);
    AnomalyDetector::ClearAll();
}

// ---------------------------------------------------------------------------
// MessageAsyncClient 损坏载荷必须 fail-closed
// ---------------------------------------------------------------------------

TEST(CurrencyTest, RedisLoadCorruptPayloadFailsClosed)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int successCount = 0;
    int failureCount = 0;
    Guid failedKey = 0;
    auto failureReason = Client::LoadFailureReason::DataNotFound;
    client.SetLoadCallback([&](Guid, CurrencyComp&) { ++successCount; });
    client.SetLoadFailedCallback([&](Guid key, Client::LoadFailureReason reason) {
        ++failureCount;
        failedKey = key;
        failureReason = reason;
        EXPECT_EQ(0u, client.in_flight_load_count());
        EXPECT_EQ(0u, client.pending_load_count());
    });

    constexpr Guid kPlayerId = 10001;
    const auto element = Peer::TrackLoad(client, kPlayerId);
    char corruptPayload[] = {static_cast<char>(0x80)}; // 未结束的 protobuf varint。
    redisReply reply{};
    reply.type = REDIS_REPLY_STRING;
    reply.str = corruptPayload;
    reply.len = sizeof(corruptPayload);

    ASSERT_EQ(1u, client.in_flight_load_count());
    Peer::Deliver(client, &reply, element);

    EXPECT_EQ(0, successCount);
    EXPECT_EQ(1, failureCount);
    EXPECT_EQ(kPlayerId, failedKey);
    EXPECT_EQ(Client::LoadFailureReason::RedisError, failureReason);
    EXPECT_EQ(0u, client.in_flight_load_count());
    EXPECT_EQ(0u, client.pending_load_count());
    EXPECT_FALSE(element->message_value);
}

TEST(CurrencyTest, RedisLoadOversizePayloadFailsClosed)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int successCount = 0;
    int failureCount = 0;
    Guid failedKey = 0;
    auto failureReason = Client::LoadFailureReason::DataNotFound;
    client.SetLoadCallback([&](Guid, CurrencyComp&) { ++successCount; });
    client.SetLoadFailedCallback([&](Guid key, Client::LoadFailureReason reason) {
        ++failureCount;
        failedKey = key;
        failureReason = reason;
        EXPECT_EQ(0u, client.in_flight_load_count());
        EXPECT_EQ(0u, client.pending_load_count());
    });

    constexpr Guid kPlayerId = 10002;
    const auto element = Peer::TrackLoad(client, kPlayerId);
    char sentinel = 0;
    redisReply reply{};
    reply.type = REDIS_REPLY_STRING;
    reply.str = &sentinel;
    reply.len = static_cast<size_t>(std::numeric_limits<int>::max()) + 1u;

    ASSERT_EQ(1u, client.in_flight_load_count());
    Peer::Deliver(client, &reply, element);

    EXPECT_EQ(0, successCount);
    EXPECT_EQ(1, failureCount);
    EXPECT_EQ(kPlayerId, failedKey);
    EXPECT_EQ(Client::LoadFailureReason::RedisError, failureReason);
    EXPECT_EQ(0u, client.in_flight_load_count());
    EXPECT_EQ(0u, client.pending_load_count());
    EXPECT_FALSE(element->message_value);
}

// 2026-10-16:在途 GET 收到空应答(连接断开 / 被释放时 hiredis 的一次性回调)不是读失败。
// 以前直接 FailLoad(RedisError),玩家被踢回登录界面;现在放回重试队列等重连重发,且不消耗 NIL 重试次数。
TEST(CurrencyTest, RedisLoadNullReplyRequeuesInsteadOfFailing)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int successCount = 0;
    int failureCount = 0;
    client.SetLoadCallback([&](Guid, CurrencyComp&) { ++successCount; });
    client.SetLoadFailedCallback([&](Guid, Client::LoadFailureReason) { ++failureCount; });

    constexpr Guid kPlayerId = 10003;
    const auto element = Peer::TrackLoad(client, kPlayerId);
    element->retry_count = 3; // 已经因 NIL 退避过 3 次
    ASSERT_EQ(1u, client.in_flight_load_count());

    Peer::Deliver(client, nullptr, element);

    EXPECT_EQ(0, successCount);
    EXPECT_EQ(0, failureCount);
    EXPECT_EQ(0u, client.in_flight_load_count());
    ASSERT_EQ(1u, client.pending_load_count());
    const auto requeued = Peer::PendingLoad(client, kPlayerId);
    ASSERT_NE(nullptr, requeued);
    EXPECT_EQ(element, requeued);
    EXPECT_EQ(3, requeued->retry_count); // 空应答不计入 NIL 重试预算

    // 同一份在途的重复空回调(已不在在途表里)按陈旧回调丢弃:不重复入队,也不判失败。
    Peer::Deliver(client, nullptr, element);
    EXPECT_EQ(0, failureCount);
    EXPECT_EQ(1u, client.pending_load_count());
}

// 断线期间又调过 AsyncLoad、同 key 已有排队项时,迟到的空回调不覆盖那一项,
// 只把它的重试次数抬到两者较大者(NIL 退避预算不被外部重触发重置)。
TEST(CurrencyTest, RedisLoadNullReplyKeepsExistingPendingEntry)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis; // 未连接:AsyncLoad 只会排队
    Client client(hiredis);
    int failureCount = 0;
    client.SetLoadFailedCallback([&](Guid, Client::LoadFailureReason) { ++failureCount; });

    constexpr Guid kPlayerId = 10004;
    const auto inFlight = Peer::TrackLoad(client, kPlayerId);
    inFlight->retry_count = 2;
    client.AsyncLoad(kPlayerId); // retry_count = 0
    const auto queued = Peer::PendingLoad(client, kPlayerId);
    ASSERT_NE(nullptr, queued);
    ASSERT_NE(inFlight, queued);
    ASSERT_EQ(0, queued->retry_count);

    Peer::Deliver(client, nullptr, inFlight);

    EXPECT_EQ(0, failureCount);
    EXPECT_EQ(0u, client.in_flight_load_count());
    EXPECT_EQ(1u, client.pending_load_count());
    EXPECT_EQ(queued, Peer::PendingLoad(client, kPlayerId));
    EXPECT_EQ(2, queued->retry_count);
}

// ERROR 是服务端的答复,重发也是同一个答复:仍按 RedisError fail-closed,不进重试队列。
TEST(CurrencyTest, RedisLoadErrorReplyStillFailsClosed)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int failureCount = 0;
    auto failureReason = Client::LoadFailureReason::DataNotFound;
    client.SetLoadFailedCallback([&](Guid, Client::LoadFailureReason reason) {
        ++failureCount;
        failureReason = reason;
    });

    constexpr Guid kPlayerId = 10005;
    const auto element = Peer::TrackLoad(client, kPlayerId);
    char errorText[] = "WRONGTYPE Operation against a key holding the wrong kind of value";
    redisReply reply{};
    reply.type = REDIS_REPLY_ERROR;
    reply.str = errorText;
    reply.len = sizeof(errorText) - 1;

    Peer::Deliver(client, &reply, element);

    EXPECT_EQ(1, failureCount);
    EXPECT_EQ(Client::LoadFailureReason::RedisError, failureReason);
    EXPECT_EQ(0u, client.in_flight_load_count());
    EXPECT_EQ(0u, client.pending_load_count());
}

// ---------------------------------------------------------------------------
// MessageAsyncClient 带归属校验的存盘(reentry-barrier §6.2 方案 a)
// ---------------------------------------------------------------------------

// guard 不等 → Lua 返回 0:是终态"这份期望值已作废",不是故障。必须只回 rejected 回调,
// 不回成功、不回失败、不重试,且同 key 排队中**期望值相同**的更新值也一并丢弃。
TEST(CurrencyTest, RedisGuardedSaveRejectedIsTerminalAndDropsPending)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    int failedCount = 0;
    int rejectedCount = 0;
    Guid rejectedKey = 0;
    std::string rejectedRedisKey;
    std::string rejectedGuard;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { ++savedCount; });
    client.SetSaveFailedCallback([&](Guid, const std::string&, int) { ++failedCount; });
    client.SetSaveRejectedCallback([&](Guid key, const std::string& redisKey, const std::string& guardExpected) {
        ++rejectedCount;
        rejectedKey = key;
        rejectedRedisKey = redisKey;
        rejectedGuard = guardExpected;
        // 回调时队列必须已经清干净:调用方会在回调里销毁实体,之后不能再有任何
        // 针对该 key 的写入冒出来。
        EXPECT_EQ(0u, client.in_flight_save_count());
        EXPECT_EQ(0u, client.pending_save_count());
    });

    constexpr Guid kPlayerId = 10003;
    const std::string guardKey = "player:10003:owner_epoch";
    const auto inFlight = Peer::TrackGuardedSave(client, kPlayerId, guardKey, "7");
    // 同 key 的后续存盘在前一条回包前到达,按既有规则排进 pending(不碰 hiredis)。
    client.Save(std::make_shared<CurrencyComp>(), kPlayerId, guardKey, "7");
    ASSERT_EQ(1u, client.in_flight_save_count());
    ASSERT_EQ(1u, client.pending_save_count());

    redisReply reply{};
    reply.type = REDIS_REPLY_INTEGER;
    reply.integer = 0;
    Peer::DeliverSaved(client, &reply, inFlight);

    EXPECT_EQ(0, savedCount);
    EXPECT_EQ(0, failedCount);
    EXPECT_EQ(1, rejectedCount);
    EXPECT_EQ(kPlayerId, rejectedKey);
    EXPECT_EQ(inFlight->redis_key, rejectedRedisKey);
    // 调用方要拿"被拒的那一份期望值"与实体当前缓存的 epoch 比,才能区分
    // "自己被废黜"与"只是一笔旧代际的在途写"。
    EXPECT_EQ("7", rejectedGuard);
    EXPECT_EQ(0u, client.in_flight_save_count());
    EXPECT_EQ(0u, client.pending_save_count());
}

// 在途存盘(guard=7)被拒时,排队中的值已经带着更新的归属(guard=8):调用方在这笔写
// 在途期间拿到了新 epoch,仍是合法持有者。那份更新的快照必须照常发出,不能被当成
// "同一份已作废的归属"丢掉;此时也不回 rejected(那笔旧写已被它取代)。
TEST(CurrencyTest, RedisGuardedSaveRejectedKeepsPendingValueWithNewerGuard)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    int rejectedCount = 0;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { ++savedCount; });
    client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) { ++rejectedCount; });

    constexpr Guid kPlayerId = 10005;
    const std::string guardKey = "player:10005:owner_epoch";
    const auto inFlight = Peer::TrackGuardedSave(client, kPlayerId, guardKey, "7");
    client.Save(std::make_shared<CurrencyComp>(), kPlayerId, guardKey, "8");
    ASSERT_EQ(1u, client.in_flight_save_count());
    ASSERT_EQ(1u, client.pending_save_count());

    redisReply reply{};
    reply.type = REDIS_REPLY_INTEGER;
    reply.integer = 0;
    Peer::DeliverSaved(client, &reply, inFlight);

    EXPECT_EQ(0, savedCount);
    EXPECT_EQ(0, rejectedCount) << "a stale in-flight write superseded by a newer-guard value is not a deposal";
    EXPECT_EQ(0u, client.in_flight_save_count());
    // 本用例没有真实 hiredis 连接,IssueSave 会把它放回 pending 等重连;关键是它**还在**。
    EXPECT_EQ(1u, client.pending_save_count());
}

// guard 相等 → Lua 返回 1:与无 guard 的存盘完全一样走成功回调。
TEST(CurrencyTest, RedisGuardedSaveAcceptedCompletesNormally)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    int rejectedCount = 0;
    Guid savedKey = 0;
    client.SetSaveCallback([&](Guid key, CurrencyComp&) {
        ++savedCount;
        savedKey = key;
    });
    client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) { ++rejectedCount; });

    constexpr Guid kPlayerId = 10004;
    const auto inFlight = Peer::TrackGuardedSave(client, kPlayerId, "player:10004:owner_epoch", "7");
    ASSERT_EQ(1u, client.in_flight_save_count());

    redisReply reply{};
    reply.type = REDIS_REPLY_INTEGER;
    reply.integer = 1;
    Peer::DeliverSaved(client, &reply, inFlight);

    EXPECT_EQ(1, savedCount);
    EXPECT_EQ(kPlayerId, savedKey);
    EXPECT_EQ(0, rejectedCount);
    EXPECT_EQ(0u, client.in_flight_save_count());
    EXPECT_EQ(0u, client.pending_save_count());
}

// ---------------------------------------------------------------------------
// 存盘队列的新旧顺序与归属围栏(帮会二期 B4c,docs/design/guild-phase2/08-save-owner-fence.md §8.2.2 / §8.2.3)
//
// 同 key 写入的不变量:在途至多一份、排队至多一份,且排队的永远比在途新。修复前唯一破坏它的入口是
// EnqueueSave"无在途、排队里有一份失败后等退避的旧值"时直接发新值 —— 之后成功 / 失败 / 重连三条
// 路径都把那份旧值当"更新的值"发出去,盖掉新值。
// ---------------------------------------------------------------------------
namespace
{
std::shared_ptr<CurrencyComp> MarkedCurrency(uint64_t marker)
{
    auto value = std::make_shared<CurrencyComp>();
    value->add_values(marker);
    return value;
}

// Lua 把既有 DBTask 视为不透明字节,这里带 NUL 以守住 hiredis 的二进制参数契约。
std::shared_ptr<const RedisSaveCommitIntent> CommitIntentForTest(Guid playerId)
{
    auto intent = std::make_shared<RedisSaveCommitIntent>();
    intent->index_key = "player:save_pending:1:" + std::to_string(playerId % RedisSaveCommitIntent::kIndexBuckets);
    intent->topic = "db_task_zone_1";
    intent->player_database_task = std::string("first\0task", 10);
    intent->player_database_1_task = std::string("second\0task", 11);
    return intent;
}

// 严格屏障的覆盖世代参数(ARGV[8..11]),取合法的中性值;C3 本身由真 Redis 用例覆盖。
RedisSaveSchemaGen SchemaGenForTest()
{
    RedisSaveSchemaGen schemaGen;
    schemaGen.output_gen = 1;
    schemaGen.load_gen = 1;
    schemaGen.mode = "warn";
    schemaGen.writer = "writer-unit-test";
    return schemaGen;
}

uint64_t MarkerOf(const CurrencyComp& value)
{
    return value.values_size() > 0 ? value.values(0) : 0;
}

redisReply IntegerReply(long long value)
{
    redisReply reply{};
    reply.type = REDIS_REPLY_INTEGER;
    reply.integer = value;
    return reply;
}

// 一个普通的写失败(不是 NOSCRIPT,NOSCRIPT 走的是原地重发)。
redisReply ErrorReply()
{
    static char kErr[] = "ERR injected by test";
    redisReply reply{};
    reply.type = REDIS_REPLY_ERROR;
    reply.str = kErr;
    reply.len = std::strlen(kErr);
    return reply;
}
} // namespace

// Q1a:在途 S1 写失败进排队等退避;此时来了新值 M(无在途)。M 顶替 S1,并继承它的退避进度
// (retry_count / next_retry_at / save_failure_notified)。
// 本用例没有连接,测不到"已连接时绕过排队直接发出"那条颠倒本身(未连接时修复前 M 也只会进排队);
// 修复前它红在三个继承字段上。颠倒本身由 Q3(真 Redis,RedisConnectedSaveDoesNotBypassFailedPending)覆盖。
TEST(CurrencyTest, RedisSaveSupersedesFailedPendingAndInheritsBackoff)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { ++savedCount; });

    constexpr Guid kPlayerId = 10011;
    const auto s1 = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
    auto error = ErrorReply();
    Peer::DeliverSaved(client, &error, s1);

    const auto failed = Peer::PendingSave(client, kPlayerId);
    ASSERT_EQ(s1, failed) << "写失败的那份进排队等退避";
    ASSERT_EQ(0u, client.in_flight_save_count());
    ASSERT_GT(failed->retry_count, 0);
    failed->save_failure_notified = true; // 模拟"这段故障已经告过警"
    const int inheritedRetryCount = failed->retry_count;
    const auto inheritedDueAt = failed->next_retry_at;

    client.Save(MarkedCurrency(2), kPlayerId);

    const auto pending = Peer::PendingSave(client, kPlayerId);
    ASSERT_NE(nullptr, pending);
    EXPECT_EQ(2u, MarkerOf(*pending->message_value)) << "新值顶替了排队的旧值";
    EXPECT_EQ(inheritedRetryCount, pending->retry_count) << "继承退避进度,不重置";
    EXPECT_EQ(inheritedDueAt, pending->next_retry_at) << "沿用旧值的到期时刻,交给定时器发";
    EXPECT_TRUE(pending->save_failure_notified) << "同一段故障不重复告警";
    EXPECT_EQ(1u, client.pending_save_count());
    EXPECT_EQ(0u, client.in_flight_save_count());
    EXPECT_EQ(0, savedCount);
}

// Q1b:接 Q1a,到期发出的是 M,落地回调带的是 M,之后 S1 再也不会被发出。
TEST(CurrencyTest, RedisSaveSupersededPendingPersistsNewestOnly)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    uint64_t savedMarker = 0;
    client.SetSaveCallback([&](Guid, CurrencyComp& value) {
        ++savedCount;
        savedMarker = MarkerOf(value);
    });

    constexpr Guid kPlayerId = 10012;
    const auto s1 = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
    auto error = ErrorReply();
    Peer::DeliverSaved(client, &error, s1);
    client.Save(MarkedCurrency(2), kPlayerId);

    const auto issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, issued);

    EXPECT_EQ(1, savedCount);
    EXPECT_EQ(2u, savedMarker) << "落地并回调的是新值";
    EXPECT_EQ(0u, client.pending_save_count()) << "旧值 S1 再也不会被发出";
    EXPECT_EQ(0u, client.in_flight_save_count());
}

// Q1c:顶替后的 M 发出又失败,重新排队的仍是 M,最终落地的也是 M(不是 S1)。
TEST(CurrencyTest, RedisSaveSupersededPendingRetriesNewestAfterError)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int savedCount = 0;
    uint64_t savedMarker = 0;
    client.SetSaveCallback([&](Guid, CurrencyComp& value) {
        ++savedCount;
        savedMarker = MarkerOf(value);
    });

    constexpr Guid kPlayerId = 10013;
    const auto s1 = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
    auto error = ErrorReply();
    Peer::DeliverSaved(client, &error, s1);
    client.Save(MarkedCurrency(2), kPlayerId);

    const auto firstTry = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, firstTry);
    Peer::DeliverSaved(client, &error, firstTry);

    const auto requeued = Peer::PendingSave(client, kPlayerId);
    ASSERT_NE(nullptr, requeued);
    EXPECT_EQ(2u, MarkerOf(*requeued->message_value)) << "失败后重新排队的是新值,不是 S1";

    const auto secondTry = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, secondTry);
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, secondTry);

    EXPECT_EQ(1, savedCount);
    EXPECT_EQ(2u, savedMarker);
    EXPECT_EQ(0u, client.pending_save_count());
    EXPECT_EQ(0u, client.in_flight_save_count());
}

// 每份实写成功的快照立即准入 SQL,只有未下发就被合并的中间值不执行;全局退出回调仍等最新值。
TEST(CurrencyTest, RedisCommittedHooksFollowActualWritesBeforeNewestSaveCallback)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    std::vector<int> events;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { events.push_back(3); });
    constexpr Guid kPlayerId = 10021;
    const auto old = Peer::TrackGuardedSave(client, kPlayerId, "player:10021:owner_epoch", "7");
    old->barrier_key = "player:{10021}:save_barrier";
    old->barrier_version = "00000000000000000007:00000000000000000001";
    old->commit_intent = CommitIntentForTest(kPlayerId);
    old->committed_callback = [&] { events.push_back(1); };
    client.SaveWithCommitBarrier(MarkedCurrency(2), kPlayerId, "player:10021:owner_epoch", "7",
        "player:{10021}:save_barrier", "00000000000000000007:00000000000000000002", CommitIntentForTest(kPlayerId), SchemaGenForTest(), [&] { events.push_back(4); });
    client.SaveWithCommitBarrier(MarkedCurrency(3), kPlayerId, "player:10021:owner_epoch", "7",
        "player:{10021}:save_barrier", "00000000000000000007:00000000000000000003", CommitIntentForTest(kPlayerId), SchemaGenForTest(), [&] { events.push_back(2); });
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, old);
    EXPECT_EQ((std::vector<int>{1}), events) << "A 已提交而 B 未成功,此时崩溃也必须有 A 的 SQL 任务";
    const auto latest = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, latest);
    EXPECT_EQ("player:{10021}:save_barrier", latest->barrier_key);
    EXPECT_EQ("00000000000000000007:00000000000000000003", latest->barrier_version);
    ASSERT_NE(nullptr, latest->commit_intent);
    EXPECT_EQ(CommitIntentForTest(kPlayerId)->player_database_task, latest->commit_intent->player_database_task);
    EXPECT_EQ(CommitIntentForTest(kPlayerId)->player_database_1_task, latest->commit_intent->player_database_1_task);
    Peer::DeliverSaved(client, &ok, latest);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), events) << "未下发的中间值不能准入 SQL";
    Peer::DeliverSaved(client, &ok, old);
    Peer::DeliverSaved(client, &ok, latest);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), events) << "旧连接与重复回包不能再次准入 SQL";
}

TEST(CurrencyTest, RedisCommitBarrierErrorsNeverAdmitSql)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int committed = 0;
    int saved = 0;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { ++saved; });
    constexpr Guid kPlayerId = 10022;
    client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10022:owner_epoch", "7",
        "player:{10022}:save_barrier", "00000000000000000007:00000000000000000001", CommitIntentForTest(kPlayerId), SchemaGenForTest(), [&] { ++committed; });
    auto issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    Peer::DeliverSaved(client, nullptr, issued);
    auto error = ErrorReply();
    issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    Peer::DeliverSaved(client, &error, issued);
    // 严格屏障的合法整数只有 0 / 1 / 2 / 3(2 = warn 模式 C3 照写,是提交证据;3 = enforce 拒写),
    // 其余整数仍是坏回包,不能准入 SQL。
    auto malformedAck = IntegerReply(4);
    issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    Peer::DeliverSaved(client, &malformedAck, issued);
    EXPECT_EQ(0, committed);
    EXPECT_EQ(0, saved);
    EXPECT_EQ(1u, client.pending_save_count());
    issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, issued);
    EXPECT_EQ(1, committed);
    EXPECT_EQ(1, saved);
}

TEST(CurrencyTest, RedisCommittedHookCanReenterSaveWithoutPublishingOldCompletion)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    std::vector<int> events;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { events.push_back(3); });
    constexpr Guid kPlayerId = 10024;
    const auto old = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
    old->committed_callback = [&] {
        EXPECT_EQ(1u, client.in_flight_save_count()) << "钩子期间保留在途登记,重入同 key 只能排队";
        events.push_back(1);
        client.Save(MarkedCurrency(2), kPlayerId, [&] { events.push_back(2); });
        // 模拟重入其他玩家已连接的 Save 导致 saving_queue_ 扩容,不能跨钩子保存 iterator。
        for (Guid other = 20000; other < 20100; ++other)
        {
            Peer::TrackSave(client, other, MarkedCurrency(9));
        }
    };
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, old);
    EXPECT_EQ((std::vector<int>{1}), events);
    const auto latest = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, latest);
    EXPECT_EQ(2u, MarkerOf(*latest->message_value));
    Peer::DeliverSaved(client, &ok, latest);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), events);
    Peer::DeliverSaved(client, &ok, old);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), events);
}

TEST(CurrencyTest, RedisCommitBarrierRejectionDiscardsSameOwnerHooks)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int committed = 0;
    int rejected = 0;
    client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) { ++rejected; });
    constexpr Guid kPlayerId = 10023;
    const auto old = Peer::TrackGuardedSave(client, kPlayerId, "player:10023:owner_epoch", "7");
    old->committed_callback = [&] { ++committed; };
    client.SaveWithCommitBarrier(MarkedCurrency(2), kPlayerId, "player:10023:owner_epoch", "7",
        "player:{10023}:save_barrier", "00000000000000000007:00000000000000000002", CommitIntentForTest(kPlayerId), SchemaGenForTest(), [&] { ++committed; });
    auto no = IntegerReply(0);
    Peer::DeliverSaved(client, &no, old);
    EXPECT_EQ(0, committed);
    EXPECT_EQ(1, rejected);
    EXPECT_EQ(0u, client.pending_save_count());
}

TEST(CurrencyTest, RedisCommitBarrierRequiresCompleteOutboxBeforeQueueing)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int committed = 0;
    constexpr Guid kPlayerId = 10025;
    const auto enqueue = [&](std::shared_ptr<const RedisSaveCommitIntent> intent) {
        return client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10025:owner_epoch", "7",
            "player:{10025}:save_barrier", "00000000000000000007:00000000000000000001", std::move(intent), SchemaGenForTest(), [&] { ++committed; });
    };
    EXPECT_FALSE(enqueue({}));
    for (int field = 0; field < 4; ++field)
    {
        auto intent = std::make_shared<RedisSaveCommitIntent>(*CommitIntentForTest(kPlayerId));
        if (field == 0) intent->index_key.clear();
        if (field == 1) intent->topic.clear();
        if (field == 2) intent->player_database_task.clear();
        if (field == 3) intent->player_database_1_task.clear();
        EXPECT_FALSE(enqueue(std::move(intent))) << "缺失字段=" << field;
    }
    EXPECT_EQ(0u, client.pending_save_count());
    EXPECT_EQ(0u, client.in_flight_save_count());
    EXPECT_EQ(0, committed);
    EXPECT_TRUE(enqueue(CommitIntentForTest(kPlayerId)));
    EXPECT_EQ(1u, client.pending_save_count());
}

// ── 覆盖世代校验 C3 的异步结局(docs/design/mixed-version-persist-field-protection.md §5.2、§10)──────────────

// 返回 2(warn 模式判定倒退但已照写)是提交证据:照常准入 SQL、发布落地回调;另通知一次计数,且通知先于提交钩子。
TEST(CurrencyTest, RedisCommitBarrierSchemaWarnedIsCommitEvidence)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    std::vector<int> events;
    client.SetSaveSchemaWarnedCallback([&](Guid, const std::string&) { events.push_back(1); });
    client.SetSaveCallback([&](Guid, CurrencyComp&) { events.push_back(3); });
    int rejected = 0;
    client.SetSaveSchemaRejectedCallback([&](Guid, const std::string&, const std::string&) { ++rejected; });
    constexpr Guid kPlayerId = 10026;
    ASSERT_TRUE(client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10026:owner_epoch", "7",
        "player:{10026}:save_barrier", "00000000000000000007:00000000000000000001", CommitIntentForTest(kPlayerId),
        SchemaGenForTest(), [&] { events.push_back(2); }));
    const auto issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    auto warned = IntegerReply(2);
    Peer::DeliverSaved(client, &warned, issued);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), events);
    EXPECT_EQ(0, rejected);
    EXPECT_EQ(0u, client.pending_save_count());
    EXPECT_EQ(0u, client.in_flight_save_count());
}

// 2 / 3 只对严格屏障元素有含义;旧 guard 路径收到它们仍是坏回包,按失败重试,不准入 SQL、不回调。
TEST(CurrencyTest, RedisSchemaRepliesOnNonBarrierSavesAreNotEvidence)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    constexpr Guid kPlayerId = 10027;
    for (const long long value : {2LL, 3LL})
    {
        Client::HiredisPtr hiredis;
        Client client(hiredis);
        int committed = 0;
        int saved = 0;
        int warned = 0;
        int schemaRejected = 0;
        int ownerRejected = 0;
        client.SetSaveCallback([&](Guid, CurrencyComp&) { ++saved; });
        client.SetSaveSchemaWarnedCallback([&](Guid, const std::string&) { ++warned; });
        client.SetSaveSchemaRejectedCallback([&](Guid, const std::string&, const std::string&) { ++schemaRejected; });
        client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) { ++ownerRejected; });
        const auto guarded = Peer::TrackGuardedSave(client, kPlayerId, "player:10027:owner_epoch", "7");
        guarded->committed_callback = [&] { ++committed; };
        auto reply = IntegerReply(value);
        Peer::DeliverSaved(client, &reply, guarded);
        EXPECT_EQ(0, committed) << "回包 " << value;
        EXPECT_EQ(0, saved) << "回包 " << value;
        EXPECT_EQ(0, warned) << "回包 " << value;
        EXPECT_EQ(0, schemaRejected) << "回包 " << value;
        EXPECT_EQ(0, ownerRejected) << "回包 " << value;
        EXPECT_EQ(1u, client.pending_save_count()) << "回包 " << value << " 按坏回包重新排队";
        EXPECT_EQ(0u, client.in_flight_save_count());
        // 重试收到真正的 1 才准入。
        const auto retried = Peer::IssuePendingForTest(client, kPlayerId);
        ASSERT_NE(nullptr, retried);
        auto ok = IntegerReply(1);
        Peer::DeliverSaved(client, &ok, retried);
        EXPECT_EQ(1, committed) << "回包 " << value;
        EXPECT_EQ(1, saved) << "回包 " << value;
    }
}

// 返回 3(enforce 拒写):不重试、不准入 SQL、不发布落地回调;同 guard 的排队值一并丢弃,走 C3 专用回调,
// 不走"已废黜"回调。
TEST(CurrencyTest, RedisCommitBarrierSchemaRejectedDiscardsSameOwnerHooks)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int committed = 0;
    int saved = 0;
    int ownerRejected = 0;
    std::vector<std::string> schemaRejected;
    client.SetSaveCallback([&](Guid, CurrencyComp&) { ++saved; });
    client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) { ++ownerRejected; });
    client.SetSaveSchemaRejectedCallback([&](Guid, const std::string&, const std::string& guard) { schemaRejected.push_back(guard); });
    constexpr Guid kPlayerId = 10028;
    ASSERT_TRUE(client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10028:owner_epoch", "7",
        "player:{10028}:save_barrier", "00000000000000000007:00000000000000000001", CommitIntentForTest(kPlayerId),
        SchemaGenForTest(), [&] { ++committed; }));
    const auto first = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, first);
    ASSERT_TRUE(client.SaveWithCommitBarrier(MarkedCurrency(2), kPlayerId, "player:10028:owner_epoch", "7",
        "player:{10028}:save_barrier", "00000000000000000007:00000000000000000002", CommitIntentForTest(kPlayerId),
        SchemaGenForTest(), [&] { ++committed; }));
    ASSERT_EQ(1u, client.pending_save_count());
    auto refused = IntegerReply(3);
    Peer::DeliverSaved(client, &refused, first);
    EXPECT_EQ(0, committed);
    EXPECT_EQ(0, saved);
    EXPECT_EQ(0, ownerRejected) << "C3 拒写不是废黜";
    EXPECT_EQ((std::vector<std::string>{"7"}), schemaRejected);
    EXPECT_EQ(0u, client.pending_save_count()) << "同 guard 的排队值再发只会再被拒,一并丢弃";
    EXPECT_EQ(0u, client.in_flight_save_count()) << "不重试";
    Peer::DeliverSaved(client, &refused, first);
    EXPECT_EQ(1u, schemaRejected.size()) << "迟到的重复回包被当作陈旧回调忽略";
}

// 返回 3 时排队值带着不同的 guard(在途期间拿到了更新的归属):照 0 分支的做法照发,不回调。
TEST(CurrencyTest, RedisCommitBarrierSchemaRejectedIssuesPendingWithNewerGuard)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    int schemaRejected = 0;
    client.SetSaveSchemaRejectedCallback([&](Guid, const std::string&, const std::string&) { ++schemaRejected; });
    constexpr Guid kPlayerId = 10029;
    ASSERT_TRUE(client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10029:owner_epoch", "7",
        "player:{10029}:save_barrier", "00000000000000000007:00000000000000000001", CommitIntentForTest(kPlayerId),
        SchemaGenForTest()));
    const auto first = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, first);
    ASSERT_TRUE(client.SaveWithCommitBarrier(MarkedCurrency(2), kPlayerId, "player:10029:owner_epoch", "8",
        "player:{10029}:save_barrier", "00000000000000000008:00000000000000000002", CommitIntentForTest(kPlayerId),
        SchemaGenForTest()));
    auto refused = IntegerReply(3);
    Peer::DeliverSaved(client, &refused, first);
    EXPECT_EQ(0, schemaRejected) << "被更新归属的排队值取代,由它的结果决定后续";
    // 没有连接:IssueSave 把它放回排队,等定时器发出。
    const auto next = Peer::PendingSave(client, kPlayerId);
    ASSERT_NE(nullptr, next);
    EXPECT_EQ("8", next->guard_expected);
    EXPECT_EQ(2u, MarkerOf(*next->message_value));
}

// 覆盖世代参数随 element 走(重试、NOSCRIPT 重发、重连回收发同一组参数),形状非法时入队前就拒绝:
// 否则脚本返回 error_reply,OnSaved 对 ERROR 无限重试,这名玩家永远存不进盘。
TEST(CurrencyTest, RedisCommitBarrierCarriesAndValidatesSchemaArguments)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;
    Client::HiredisPtr hiredis;
    Client client(hiredis);
    constexpr Guid kPlayerId = 10030;
    const auto enqueue = [&](const RedisSaveSchemaGen& schemaGen) {
        return client.SaveWithCommitBarrier(MarkedCurrency(1), kPlayerId, "player:10030:owner_epoch", "7",
            "player:{10030}:save_barrier", "00000000000000000007:00000000000000000001", CommitIntentForTest(kPlayerId), schemaGen);
    };
    const auto with = [](uint32_t output, uint32_t load, std::string mode, std::string writer) {
        RedisSaveSchemaGen schemaGen;
        schemaGen.output_gen = output;
        schemaGen.load_gen = load;
        schemaGen.mode = std::move(mode);
        schemaGen.writer = std::move(writer);
        return schemaGen;
    };
    const std::vector<RedisSaveSchemaGen> invalid{
        with(0, 0, "warn", "w"), with(1, 0, "warn", "w"), with(2, 3, "warn", "w"), with(2, 1, "", "w"),
        with(2, 1, "Warn", "w"), with(2, 1, "enforce ", "w"), with(2, 1, "warn", ""),
        with(2, 1, "warn", std::string(65, 'w')), with(2, 1, "warn", "bad writer"), with(2, 1, "warn", "bad_writer"),
    };
    for (const auto& schemaGen : invalid)
    {
        EXPECT_FALSE(enqueue(schemaGen)) << schemaGen.output_gen << "/" << schemaGen.load_gen << "/'" << schemaGen.mode
                                         << "'/'" << schemaGen.writer << "'";
    }
    EXPECT_EQ(0u, client.pending_save_count());
    ASSERT_TRUE(enqueue(with(4294967295u, 3, "enforce", std::string(64, 'a'))));
    const auto queued = Peer::PendingSave(client, kPlayerId);
    ASSERT_NE(nullptr, queued);
    EXPECT_EQ("4294967295", queued->schema_output_gen);
    EXPECT_EQ("3", queued->schema_load_gen);
    EXPECT_EQ("enforce", queued->schema_mode);
    EXPECT_EQ(std::string(64, 'a'), queued->schema_writer);
    // 写方令牌生成器产出的令牌满足同一条形状校验,且每次不同(令牌必须不能被再次加载复现)。
    const std::string tokenA = player_persist_writer::NewToken();
    const std::string tokenB = player_persist_writer::NewToken();
    EXPECT_TRUE(IsValidRedisSaveSchemaWriter(tokenA)) << tokenA;
    EXPECT_NE(tokenA, tokenB);
}

// 部署键 PersistSchemaRegressionMode 的归一化:缺键 / 空串 = warn;只认两个字面量(区分大小写);其余拒绝启动。
TEST(CurrencyTest, PersistSchemaRegressionModeNormalization)
{
    using persist_schema_regression_mode::Normalize;
    EXPECT_EQ(std::optional<std::string_view>("warn"), Normalize(""));
    EXPECT_EQ(std::optional<std::string_view>("warn"), Normalize("warn"));
    EXPECT_EQ(std::optional<std::string_view>("enforce"), Normalize("enforce"));
    for (const std::string_view bad : {"Warn", "ENFORCE", " warn", "enforce ", "off", "true", "0"})
    {
        EXPECT_EQ(std::nullopt, Normalize(bad)) << "'" << bad << "'";
    }
}

namespace
{
// currency_test 不链接节点生命周期宿主;沿源码契约守住“缓存丢失不建空号”和唯一 SQL 准入点。
std::string ReadPlayerLifecycleSourceForContract()
{
    const std::filesystem::path relative = "cpp/libs/services/scene/player/system/player_lifecycle.cpp";
    for (auto root : {std::filesystem::path(__FILE__).parent_path(), std::filesystem::current_path()})
    {
        while (!root.empty())
        {
            std::ifstream stream(root / relative, std::ios::binary);
            if (stream)
            {
                std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
                source.erase(std::remove(source.begin(), source.end(), '\r'), source.end());
                return source;
            }
            const auto parent = root.parent_path();
            if (parent == root) break;
            root = parent;
        }
    }
    return {};
}
} // namespace

TEST(CurrencyTest, PlayerLoadFailureNeverCreatesEmptyPlayerSourceContract)
{
    const auto source = ReadPlayerLifecycleSourceForContract();
    ASSERT_FALSE(source.empty()) << "找不到生命周期源码,不能跳过资产恢复契约";
    const auto start = source.find("void PlayerLifecycleSystem::HandlePlayerAsyncLoadFailed(");
    ASSERT_NE(std::string::npos, start);
    const auto end = source.find("\nvoid PlayerLifecycleSystem::HandlePlayerAsyncSaveFailed(", start);
    ASSERT_NE(std::string::npos, end);
    const auto body = source.find('{', start);
    ASSERT_LT(body, end);
    const auto failure = source.substr(body, end - body);
    EXPECT_EQ(std::string::npos, failure.find("HandlePlayerAsyncLoaded("));
    // 类型别名可以引用 PlayerAllData,禁止的是构造空对象来伪装一次成功载入。
    EXPECT_FALSE(std::regex_search(failure,
        std::regex(R"(\bPlayerAllData\s+[A-Za-z_][A-Za-z_0-9]*\s*(?:;|\{\s*\}\s*;))")));
    EXPECT_FALSE(std::regex_search(failure, std::regex(R"(\bPlayerAllData\s*(?:\{\s*\}|\(\s*\)))")));
    EXPECT_NE(std::string::npos, failure.find("SendTipToPendingSession("));
    EXPECT_NE(std::string::npos, failure.find("EraseSessionMappingIfOwned("));
    EXPECT_NE(std::string::npos, failure.find("tlsPendingEnterMap.erase("));
    EXPECT_NE(std::string::npos, failure.find("AbandonInheritClear("));
}

TEST(CurrencyTest, PlayerLoadRejectsMissingOrForeignSubTableIdentitySourceContract)
{
    const auto source = ReadPlayerLifecycleSourceForContract();
    ASSERT_FALSE(source.empty()) << "找不到生命周期源码,不能跳过载入身份契约";
    const auto start = source.find("void PlayerLifecycleSystem::HandlePlayerAsyncLoaded(");
    ASSERT_NE(std::string::npos, start);
    const auto end = source.find("\nvoid PlayerLifecycleSystem::HandlePlayerAsyncSaved(", start);
    ASSERT_NE(std::string::npos, end);
    const auto load = source.substr(start, end - start);
    const auto reject = load.find("if (playerId == kInvalidGuid || message.player_database_data().player_id() != playerId ||");
    ASSERT_NE(std::string::npos, reject);
    const auto secondIdentity = load.find("message.player_database_1_data().player_id() != playerId", reject);
    ASSERT_NE(std::string::npos, secondIdentity);
    const auto fail = load.find("HandlePlayerAsyncLoadFailed(playerId, MessageAsyncClient<Guid, PlayerAllData>::LoadFailureReason::RedisError);", secondIdentity);
    ASSERT_NE(std::string::npos, fail);
    const auto gate = load.find("exit_release_mark::DecideInheritGate(");
    const auto consume = load.find("PlayerEnterContext ctx = std::move(pendingIt->second);");
    const auto initialize = load.find("InitPlayerFromAllData(message, ctx);");
    ASSERT_NE(std::string::npos, gate);
    ASSERT_NE(std::string::npos, consume);
    ASSERT_NE(std::string::npos, initialize);
    EXPECT_LT(fail, gate) << "损坏存档不能暂存为可恢复的 A2′ 载入结果";
    EXPECT_LT(fail, consume) << "失败处理必须还能取得待入场上下文并补写继承围栏";
    EXPECT_LT(consume, initialize);
    EXPECT_EQ(std::string::npos, load.find("set_player_id("));
    EXPECT_EQ(std::string::npos, load.find("patchedMessage"));
    EXPECT_EQ(std::string::npos, load.find("registration_timestamp()"))
        << "合法新号双 ID 已由种子提供,首次注册仍由 InitPlayerFromAllData 统一处理";
}

TEST(CurrencyTest, PlayerSqlTasksRequireParentCommitSourceContract)
{
    const auto source = ReadPlayerLifecycleSourceForContract();
    ASSERT_FALSE(source.empty()) << "找不到生命周期源码,不能跳过资产恢复契约";
    const auto start = source.find("PlayerLifecycleSystem::SaveResult PlayerLifecycleSystem::SavePlayerToRedisImpl(");
    ASSERT_NE(std::string::npos, start);
    const auto end = source.find("\nvoid PlayerLifecycleSystem::HandlePlayerSaveRejected(", start);
    ASSERT_NE(std::string::npos, end);
    const auto save = source.substr(start, end - start);
    const auto hook = save.find("auto publishDbTasks = [commitIntent,");
    ASSERT_NE(std::string::npos, hook) << "SQL 准入必须持有不可变任务原字节";
    const auto hookEnd = save.find("\n\t};\n\tif (ownerEpoch != 0)", hook);
    ASSERT_NE(std::string::npos, hookEnd);
    const auto submit = save.find("DbTaskDispatchSystem::SendDbTask(");
    ASSERT_NE(std::string::npos, submit);
    EXPECT_GT(submit, hook);
    EXPECT_LT(submit, hookEnd);
    EXPECT_EQ(std::string::npos, save.find("DbTaskDispatchSystem::SendDbTask(", submit + 1));
    const auto admitted = save.substr(hook, hookEnd - hook);
    EXPECT_NE(std::string::npos, admitted.find("sendTask(commitIntent->player_database_task);"));
    EXPECT_NE(std::string::npos, admitted.find("sendTask(commitIntent->player_database_1_task);"));
    EXPECT_EQ(std::string::npos, admitted.find("SerializeToString(")) << "提交后不能重新序列化另一份意图";
    EXPECT_NE(std::string::npos, admitted.find("DbTaskDispatchSystem::FinishSave("));
    const auto prepared = save.substr(0, hook);
    EXPECT_NE(std::string::npos, prepared.find("serializeSubTableTask(message->player_database_data(), intent->player_database_task)"));
    EXPECT_NE(std::string::npos, prepared.find("serializeSubTableTask(message->player_database_1_data(), intent->player_database_1_task)"));
    EXPECT_NE(std::string::npos, prepared.find("playerId % RedisSaveCommitIntent::kIndexBuckets"));
    EXPECT_NE(std::string::npos, prepared.find("strict save refused: home_zone missing"));
    const auto dispatch = save.substr(hookEnd);
    EXPECT_NE(std::string::npos, dispatch.find("SaveWithCommitBarrier(message, playerId,"));
    EXPECT_NE(std::string::npos, dispatch.find("\"player:{\" + playerIdStr + \"}:save_barrier\", version, commitIntent, schemaGen, std::move(publishDbTasks)"));
    // 覆盖世代参数:输出世代取统一入口盖的戳、加载世代取两表加载戳的最小值、写方令牌按实体(混版本持久字段保护 §5.2)。
    EXPECT_NE(std::string::npos, dispatch.find("schemaGen.output_gen = message->player_database_data().persist_schema_gen();"));
    EXPECT_NE(std::string::npos, dispatch.find("schemaGen.load_gen = player_persist::LoadedSchemaGen(player).value_or(0);"));
    EXPECT_NE(std::string::npos, dispatch.find("schemaGen.writer = PersistWriterToken(player);"));
    EXPECT_NE(std::string::npos, dispatch.find("Save(message, playerId, std::move(publishDbTasks))"));
}

TEST(CurrencyTest, TravelSqlBarrierRetryKeepsOriginalDeadlineAndGeneration)
{
    using namespace std::chrono;
    using Clock = travel_freeze_cap::Clock;
    const auto frozenAt = Clock::time_point{} + seconds(100);
    EXPECT_EQ(frozenAt + milliseconds(250), handoff_sql_barrier::RetryDueAt(frozenAt, frozenAt));
    EXPECT_EQ(frozenAt + seconds(35), handoff_sql_barrier::RetryDueAt(frozenAt, frozenAt + milliseconds(34900)));
    EXPECT_EQ(frozenAt + seconds(35), handoff_sql_barrier::RetryDueAt(frozenAt, frozenAt + seconds(90)))
        << "重查不能按次数滑动 35s 原始截止";
    EXPECT_TRUE(handoff_sql_barrier::IsCurrentWait(frozenAt, frozenAt, 0, false));
    EXPECT_FALSE(handoff_sql_barrier::IsCurrentWait(frozenAt, frozenAt, 0, true)) << "退出后旧 timer 不触交接";
    EXPECT_FALSE(handoff_sql_barrier::IsCurrentWait(frozenAt, frozenAt + milliseconds(1), 0, false))
        << "旧冻结代次不能触发新交接";
    EXPECT_FALSE(handoff_sql_barrier::IsCurrentWait(frozenAt, frozenAt, 123, false))
        << "另一条命令在途或已发标记时不能重发";
}

TEST(CurrencyTest, TravelHandoffMarkRequiresAtomicSqlBarrierSourceContract)
{
    const auto source = ReadPlayerLifecycleSourceForContract();
    ASSERT_FALSE(source.empty());
    const auto start = source.find("void PlayerLifecycleSystem::BeginTravelHandoff(");
    ASSERT_NE(std::string::npos, start);
    const auto end = source.find("\nvoid PlayerLifecycleSystem::HandleTravelMarkWriteRejected(", start);
    ASSERT_NE(std::string::npos, end);
    const auto begin = source.substr(start, end - start);
    EXPECT_NE(std::string::npos, begin.find("handoff_sql_barrier::kPublishMarkLua"));
    EXPECT_EQ(std::string::npos, begin.find("\"SET %s %s EX %d\"")) << "不能绕过 SQL ACK 原子复核";
    EXPECT_NE(std::string::npos, begin.find("current->frozenAtSteady != frozenAt"));
    EXPECT_NE(std::string::npos, begin.find("handoff_sql_barrier::RetryDueAt(frozenAt,"));
    EXPECT_NE(std::string::npos, begin.find("handoff_sql_barrier::IsCurrentWait(frozenAt,"));
    EXPECT_NE(std::string::npos, begin.find("current->requestedAtMs = 0;"));
    EXPECT_NE(std::string::npos, begin.find("current->markEpoch = 0;"));
    EXPECT_NE(std::string::npos, begin.find("DestroyDeposedPlayer(playerId, \"handoff_sql_owner_rejected\")"));
    const auto send = begin.find("RequestTravelEnterScene(playerId);");
    ASSERT_NE(std::string::npos, send);
    EXPECT_EQ(std::string::npos, begin.find("RequestTravelEnterScene(playerId);", send + 1));
    EXPECT_GT(send, begin.find("reply->integer != 1")) << "只有明确 1 能发 SM";
}

// Q2:HasUnsettledSave 看"在途或排队",按 key 区分,只读;在成功回调里恒为 false。
TEST(CurrencyTest, RedisHasUnsettledSaveTracksInFlightAndPending)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    constexpr Guid kPlayerId = 10014;
    constexpr Guid kOtherPlayerId = 10015;
    bool unsettledInsideCallback = true;
    client.SetSaveCallback([&](Guid key, CurrencyComp&) { unsettledInsideCallback = client.HasUnsettledSave(key); });

    EXPECT_FALSE(client.HasUnsettledSave(kPlayerId));

    const auto inFlight = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
    EXPECT_TRUE(client.HasUnsettledSave(kPlayerId)) << "仅在途";
    EXPECT_FALSE(client.HasUnsettledSave(kOtherPlayerId)) << "按 key 区分";

    client.Save(MarkedCurrency(2), kPlayerId);
    EXPECT_TRUE(client.HasUnsettledSave(kPlayerId)) << "在途 + 排队";
    EXPECT_EQ(1u, client.in_flight_save_count()) << "查询不改队列";
    EXPECT_EQ(1u, client.pending_save_count());

    // 在途那份写失败:排队里是更新的值 → 发它(未连接,回到排队)、丢掉失败的那份。
    auto error = ErrorReply();
    Peer::DeliverSaved(client, &error, inFlight);
    EXPECT_EQ(0u, client.in_flight_save_count());
    EXPECT_EQ(1u, client.pending_save_count());
    EXPECT_TRUE(client.HasUnsettledSave(kPlayerId)) << "仅排队也算未落地";

    const auto issued = Peer::IssuePendingForTest(client, kPlayerId);
    ASSERT_NE(nullptr, issued);
    auto ok = IntegerReply(1);
    Peer::DeliverSaved(client, &ok, issued);
    EXPECT_FALSE(unsettledInsideCallback) << "成功回调只在排队为空时发布,回调里查询恒为 false";
    EXPECT_FALSE(client.HasUnsettledSave(kPlayerId)) << "全部落地";
}

// Q5:守卫脚本的结构 —— 先比对归属、后写数据;缺键补种;不等在写之前返回 0。
// 不代替下面的真 Redis 用例,只保证没有 Redis 的环境也能抓住脚本被改坏。
TEST(CurrencyTest, RedisGuardScriptComparesBeforeWrite)
{
    const std::string lua = kSaveIfGuardLuaScript;
    const auto readGuard = lua.find("redis.call('GET', KEYS[2])");
    const auto writeData = lua.find("redis.call('SET', KEYS[1]");
    ASSERT_NE(std::string::npos, readGuard);
    ASSERT_NE(std::string::npos, writeData);
    EXPECT_LT(readGuard, writeData) << "先比对归属、后写数据";
    EXPECT_NE(std::string::npos, lua.find("cur == false")) << "guard 键缺失时补种(跨 zone 会话论证过的取舍)";
    const auto mismatch = lua.find("cur ~= ARGV[2]");
    ASSERT_NE(std::string::npos, mismatch);
    const auto reject = lua.find("return 0", mismatch);
    ASSERT_NE(std::string::npos, reject);
    EXPECT_LT(reject, writeData) << "归属不等必须在写数据之前返回";
}

namespace
{
std::string_view TrimLuaToken(std::string_view text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
    {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

// 整串恰好是 KEYS[<十进制下标>]。派生键(KEYS[1] .. ':x')不算声明过的键。
bool IsDeclaredKeyRef(std::string_view arg)
{
    constexpr std::string_view kPrefix = "KEYS[";
    if (!arg.starts_with(kPrefix) || !arg.ends_with(']'))
    {
        return false;
    }
    const auto index = arg.substr(kPrefix.size(), arg.size() - kPrefix.size() - 1);
    return !index.empty() && index.find_first_not_of("0123456789") == std::string_view::npos;
}

struct LuaScriptScan
{
    // 每条 redis.call / redis.pcall 按顶层逗号切开、去掉首尾空白的参数;[0] 是命令名。
    std::vector<std::vector<std::string_view>> calls;
    // 不处在任何 redis.call 命令名位置上的字符串字面量(含引号)。
    std::vector<std::string_view> strayLiterals;
};

// 只识别存盘脚本用到的 Lua 子集:单 / 双引号字符串(含反斜杠转义)、-- 行注释、圆括号嵌套。
// 长括号字符串 [[...]] 目前没有用到,不识别;以后用到要同步扩展这里。
LuaScriptScan ScanLuaScript(std::string_view lua)
{
    struct Paren
    {
        bool isRedisCall = false;
        std::size_t argBegin = 0;
        std::vector<std::string_view> args;
    };
    std::vector<Paren> parens;
    std::vector<std::string_view> literals;
    LuaScriptScan scan;
    for (std::size_t i = 0; i < lua.size(); ++i)
    {
        const char c = lua[i];
        if (c == '\'' || c == '"')
        {
            std::size_t end = i + 1;
            while (end < lua.size() && lua[end] != c)
            {
                end += (lua[end] == '\\') ? 2 : 1;
            }
            literals.push_back(lua.substr(i, end - i + 1));
            i = end;
            continue;
        }
        if (lua.compare(i, 2, "--") == 0)
        {
            const auto eol = lua.find('\n', i);
            if (eol == std::string_view::npos)
            {
                break;
            }
            i = eol;
            continue;
        }
        if (c == '(')
        {
            const auto head = lua.substr(0, i);
            parens.push_back({head.ends_with("redis.call") || head.ends_with("redis.pcall"), i + 1, {}});
            continue;
        }
        if ((c != ',' && c != ')') || parens.empty())
        {
            continue;
        }
        Paren& top = parens.back();
        if (top.isRedisCall)
        {
            top.args.push_back(TrimLuaToken(lua.substr(top.argBegin, i - top.argBegin)));
            top.argBegin = i + 1;
        }
        if (c == ')')
        {
            if (top.isRedisCall)
            {
                scan.calls.push_back(std::move(top.args));
            }
            parens.pop_back();
        }
    }
    for (const std::string_view literal : literals)
    {
        bool isCommandName = false;
        for (const auto& call : scan.calls)
        {
            isCommandName = isCommandName ||
                (call.front().data() == literal.data() && call.front().size() == literal.size());
        }
        if (!isCommandName)
        {
            scan.strayLiterals.push_back(literal);
        }
    }
    return scan;
}
} // namespace

// A0-4 回归:客户端实际发出的三段存盘脚本只访问 KEYS[] 声明的键。
// 写死的全局键在全服共享 Redis 上是所有 scene 共打的单键、随累计存盘玩家数无界增长,
// 也挡住 Redis Cluster(docs/design/capacity-model-hok-scale.md A0-4)。
// 修复前这条红:两段脚本都 SADD 一个写死的全局集合 'dirty_keys_set'。
// 判定口径(三条一起才覆盖非首参位置的写死键,如 SMOVE KEYS[1] 'g' m、DEL KEYS[1] 'g'):
//   1. 每条 redis.call 的首个参数整串恰好是 KEYS[n],派生键(KEYS[1] .. ':x')不算;
//   2. 除命令名外只有严格屏障明确枚举的类型 / 格式 / hash 字段 / 错误文案,拒绝额外键名或派生后缀;
//   3. 旧版全服集合的名字不再出现。
// 判定前提:目前每条 redis.call 的首个参数都是键;严格屏障的非键字面量逐项登记,
// 以后加无键命令(如 TIME)、选项(如 'EX')或格式比较,要同步改本用例。
// 已知盲区:经 ARGV 传入、落在非首参位置的键(RENAME KEYS[1] ARGV[2])需按命令元数判定,本用例不覆盖。
TEST(CurrencyTest, RedisSaveScriptsTouchOnlyDeclaredKeys)
{
    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    Client::HiredisPtr hiredis;
    Client client(hiredis);
    for (const std::string_view script : Peer::SaveScriptSources(client))
    {
        ASSERT_FALSE(script.empty());
        EXPECT_EQ(std::string_view::npos, script.find("dirty_keys_set"))
            << "存盘脚本仍引用旧版全服集合:\n" << script;
        const LuaScriptScan scan = ScanLuaScript(script);
        EXPECT_FALSE(scan.calls.empty()) << "没解析到任何 redis.call,判定前提已失效:\n" << script;
        for (const auto& args : scan.calls)
        {
            const std::string_view command = args.front();
            EXPECT_TRUE(command.size() >= 2 && (command.front() == '\'' || command.front() == '"'))
                << "redis.call 的命令名不是字符串字面量,判定前提已失效 [" << command << "]:\n" << script;
            if (args.size() < 2)
            {
                ADD_FAILURE() << "redis.call 没有键参数 " << command << "(无键命令要同步改本用例):\n" << script;
                continue;
            }
            EXPECT_TRUE(IsDeclaredKeyRef(args[1]))
                << "存盘脚本访问了未在 KEYS[] 声明的键 [" << args[1] << "](无键命令要同步改本用例):\n" << script;
        }
        for (const std::string_view literal : scan.strayLiterals)
        {
            const std::vector<std::string_view> barrierLiterals{
                "'string'", "':'", "'^%d+:%d+$'", "'18446744073709551615'", "'0'", "'^%d+$'", "'none'", "'hash'", "'pending'",
                "'4294967295'", "'^db_task_zone_([1-9]%d*)$'", "'^db_task_zone_([1-9]%d*)_g([1-9]%d*)$'", "'1'", "'zset'",
                "'^player:save_pending:([1-9]%d*):(%d+)$'", "'outbox_version'", "'outbox_topic'", "'outbox_player_database'", "'outbox_player_database_1'",
                "'SAVE_BARRIER invalid key type or missing owner'", "'SAVE_BARRIER invalid version'",
                "'SAVE_BARRIER invalid stored version'", "'SAVE_BARRIER version regression'", "'SAVE_BARRIER keys must differ'",
                "'SAVE_BARRIER invalid argument count'", "'SAVE_BARRIER empty key'", "'SAVE_BARRIER invalid outbox'",
                "'SAVE_BARRIER invalid bucket'", "'SAVE_BARRIER invalid stored outbox'", "'SAVE_BARRIER same version conflict'",
                "'player:'", "':owner_epoch'", "'player:{'", "'}:save_barrier'", "'SAVE_BARRIER player key mismatch'",
                // 覆盖世代校验 C3(混版本持久字段保护 §5.2):hash 字段名、模式字面量、令牌字符集与参数错误文案。
                "'schema_gen'", "'schema_writer'", "'warn'", "'enforce'", "'^[%w:%-]+$'",
                "'SAVE_BARRIER invalid schema argument'",
            };
            if (script == kSaveWithCommitBarrierLuaScript &&
                std::find(barrierLiterals.begin(), barrierLiterals.end(), literal) != barrierLiterals.end())
            {
                continue;
            }
            ADD_FAILURE() << "存盘脚本里有命令名以外的字符串字面量 " << literal
                          << "(写死键名或拼派生键;确属选项 / 比较用字面量时同步改本用例):\n" << script;
        }
    }
}

// ── 真 Redis 用例:读 MMORPG_TEST_REDIS_ADDR(host:port,host 必须是 IP),未设就跳过 ──────────
// 只指向本机 / 专用的测试 Redis,不要指向共享或预发实例:用例会写并删除自己的键。
// 清理时仍 SREM 旧版全服集合 dirty_keys_set 里本用例的成员:修复前的二进制会写入,红跑也不留残余。
namespace
{
bool TestRedisAddr(std::string& host, uint16_t& port)
{
    const char* raw = std::getenv("MMORPG_TEST_REDIS_ADDR");
    if (raw == nullptr || *raw == '\0')
    {
        return false;
    }
    const std::string addr(raw);
    const auto colon = addr.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= addr.size())
    {
        return false;
    }
    host = addr.substr(0, colon);
    const long parsed = std::strtol(addr.c_str() + colon + 1, nullptr, 10);
    if (parsed <= 0 || parsed > 65535)
    {
        return false;
    }
    port = static_cast<uint16_t>(parsed);
    return true;
}

// 随机性只隔离集成夹具身份,不参与结局或时间断言;写前还会断言三键均未占用。
Guid IsolatedRedisPlayerIdForTest()
{
    std::random_device entropy;
    const auto value = (static_cast<uint64_t>(entropy()) << 32) | static_cast<uint64_t>(entropy());
    return value == 0 ? 1 : value;
}

// 在 loop 上跑到连接回调或 3 秒超时。
std::unique_ptr<hiredis::Hiredis> ConnectForTest(muduo::net::EventLoop& loop, const std::string& host, uint16_t port)
{
    auto connection = std::make_unique<hiredis::Hiredis>(&loop, muduo::net::InetAddress(host, port));
    connection->setConnectCallback([&loop](hiredis::Hiredis*, int) { loop.quit(); });
    connection->connect();
    const auto timeout = loop.runAfter(3.0, [&loop] { loop.quit(); });
    loop.loop();
    loop.cancel(timeout);
    connection->setConnectCallback({});
    return connection;
}

// 有意泄漏:析构 Hiredis 要走 redisAsyncFree 与 Channel 注销,测试里 loop 与连接的收尾顺序不受控,没必要冒这个险。
// (third_party/muduo 原版 Hiredis 有"连接态析构断言 / 断开后二次 redisAsyncFree"的问题;本工程实际链接的
// cpp/libs/engine/muduo_windows 版已修,这里泄漏只是为了不依赖收尾顺序。)
// 做成 RAII:用例中途 ASSERT 失败提前返回时也照样泄漏,而不是在析构里断言、带崩整个测试进程。
// 声明在 connection 之后、客户端之前:客户端先析构,再放手,最后 connection 以空指针析构。
struct LeakConnectionOnExit
{
    std::unique_ptr<hiredis::Hiredis>& connection;
    ~LeakConnectionOnExit() { (void)connection.release(); }
};

// 同步连接,只用来布置与核对键。
struct SyncRedisForTest
{
    std::unique_ptr<redisContext, decltype(&redisFree)> context{nullptr, &redisFree};

    bool Connect(const std::string& host, uint16_t port)
    {
        context.reset(redisConnect(host.c_str(), port));
        return context != nullptr && context->err == 0;
    }

    // 键不存在时返回 "(nil)"。
    std::string Get(const std::string& key)
    {
        auto* reply = static_cast<redisReply*>(redisCommand(context.get(), "GET %b", key.data(), key.size()));
        std::string value = "(nil)";
        if (reply != nullptr && reply->type == REDIS_REPLY_STRING)
        {
            value.assign(reply->str, reply->len);
        }
        if (reply != nullptr)
        {
            freeReplyObject(reply);
        }
        return value;
    }

    void Set(const std::string& key, const std::string& value)
    {
        Run(redisCommand(context.get(), "SET %b %b", key.data(), key.size(), value.data(), value.size()));
    }

    void Del(const std::string& key) { Run(redisCommand(context.get(), "DEL %b", key.data(), key.size())); }

    void SRem(const std::string& set, const std::string& member)
    {
        Run(redisCommand(context.get(), "SREM %b %b", set.data(), set.size(), member.data(), member.size()));
    }

    // 下面两个回整数;连接或命令出错返回 -1,让"应为 0"的断言出错时也红,而不是误判通过。
    long long Exists(const std::string& key)
    {
        return Integer(redisCommand(context.get(), "EXISTS %b", key.data(), key.size()));
    }

    long long SIsMember(const std::string& set, const std::string& member)
    {
        return Integer(redisCommand(context.get(), "SISMEMBER %b %b", set.data(), set.size(), member.data(), member.size()));
    }

private:
    static void Run(void* reply)
    {
        if (reply != nullptr)
        {
            freeReplyObject(reply);
        }
    }

    static long long Integer(void* raw)
    {
        const auto* reply = static_cast<redisReply*>(raw);
        const long long value = (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
        Run(raw);
        return value;
    }
};
} // namespace

// 严格 Lua 的零部分写回归,不依赖异步宿主:每个错误都必须让父键与 pending 保持原样。
TEST(CurrencyTest, RedisCommitBarrierSemanticsAgainstRealRedis)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port)) GTEST_SKIP() << "需要专用 MMORPG_TEST_REDIS_ADDR";
    SyncRedisForTest sync;
    ASSERT_TRUE(sync.Connect(host, port));
    const Guid testPlayerId = IsolatedRedisPlayerIdForTest();
    const std::string member = std::to_string(testPlayerId);
    const std::string parentKey = "CurrencyComp:" + member;
    const std::string ownerKey = "player:" + member + ":owner_epoch";
    const std::string barrierKey = "player:{" + member + "}:save_barrier";
    const auto intent = CommitIntentForTest(testPlayerId);
    const std::string& indexKey = intent->index_key;
    ASSERT_EQ(0LL, sync.Exists(parentKey));
    ASSERT_EQ(0LL, sync.Exists(ownerKey));
    ASSERT_EQ(0LL, sync.Exists(barrierKey));
    struct Cleanup
    {
        SyncRedisForTest& sync;
        const std::string& parent;
        const std::string& owner;
        const std::string& barrier;
        const std::string& index;
        const std::string& member;
        ~Cleanup()
        {
            sync.Del(parent); sync.Del(owner); sync.Del(barrier);
            freeReplyObject(redisCommand(sync.context.get(), "ZREM %b %b", index.data(), index.size(), member.data(), member.size()));
        }
    } cleanup{sync, parentKey, ownerKey, barrierKey, indexKey, member};
    sync.Del(parentKey);
    sync.Del(ownerKey);
    sync.Del(barrierKey);
    using Reply = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;
    const auto version = [](const std::string& owner, const std::string& seq) {
        return std::string(20 - owner.size(), '0') + owner + ":" + std::string(20 - seq.size(), '0') + seq;
    };
    const auto setPending = [&](const std::string& value) {
        Reply cleared(static_cast<redisReply*>(redisCommand(sync.context.get(),
            "HDEL %b outbox_version outbox_topic outbox_player_database outbox_player_database_1", barrierKey.data(), barrierKey.size())), &freeReplyObject);
        ASSERT_NE(nullptr, cleared);
        ASSERT_EQ(REDIS_REPLY_INTEGER, cleared->type);
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HSET %b pending %b",
            barrierKey.data(), barrierKey.size(), value.data(), value.size())), &freeReplyObject);
        ASSERT_NE(nullptr, reply);
        ASSERT_EQ(REDIS_REPLY_INTEGER, reply->type);
    };
    const auto field = [&](const std::string& name) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HGET %b %b",
            barrierKey.data(), barrierKey.size(), name.data(), name.size())), &freeReplyObject);
        return reply != nullptr && reply->type == REDIS_REPLY_STRING ? std::string(reply->str, reply->len) : "(nil)";
    };
    const auto pending = [&]() { return field("pending"); };
    // 覆盖世代参数取中性值(输出 / 加载世代 1、warn、固定令牌):本用例只验原有屏障语义,C3 见下一个用例。
    const std::string schemaGen = "1";
    const std::string schemaMode = "warn";
    const std::string schemaWriter = "writer-semantics";
    const auto saveIntent = [&](const std::string& owner, const std::string& token, const std::string& body,
        const RedisSaveCommitIntent& incoming) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "EVAL %s 4 %b %b %b %b %b %b %b %b %b %b %b %b %b %b %b",
            kSaveWithCommitBarrierLuaScript, parentKey.data(), parentKey.size(), ownerKey.data(), ownerKey.size(),
            barrierKey.data(), barrierKey.size(), incoming.index_key.data(), incoming.index_key.size(),
            body.data(), body.size(), owner.data(), owner.size(), token.data(), token.size(),
            incoming.topic.data(), incoming.topic.size(), incoming.player_database_task.data(), incoming.player_database_task.size(),
            incoming.player_database_1_task.data(), incoming.player_database_1_task.size(), member.data(), member.size(),
            schemaGen.data(), schemaGen.size(), schemaGen.data(), schemaGen.size(), schemaMode.data(), schemaMode.size(),
            schemaWriter.data(), schemaWriter.size())), &freeReplyObject);
        if (reply == nullptr) return -99LL;
        if (reply->type == REDIS_REPLY_ERROR) return -1LL;
        return reply->type == REDIS_REPLY_INTEGER ? reply->integer : -99LL;
    };
    const auto save = [&](const std::string& owner, const std::string& token, const std::string& body) {
        return saveIntent(owner, token, body, *intent);
    };
    const auto indexScore = [&]() {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "ZSCORE %b %b",
            indexKey.data(), indexKey.size(), member.data(), member.size())), &freeReplyObject);
        return reply != nullptr && reply->type == REDIS_REPLY_STRING ? std::string(reply->str, reply->len) : "(nil)";
    };
    const auto expectFirst = [&]() {
        EXPECT_EQ("first", sync.Get(parentKey));
        EXPECT_EQ(version("5", "1"), pending());
        EXPECT_EQ(version("5", "1"), field("outbox_version"));
        EXPECT_EQ(intent->topic, field("outbox_topic"));
        EXPECT_EQ(intent->player_database_task, field("outbox_player_database"));
        EXPECT_EQ(intent->player_database_1_task, field("outbox_player_database_1"));
        EXPECT_EQ("0", indexScore());
    };

    sync.Set(ownerKey, "5");
    ASSERT_EQ(1LL, save("5", version("5", "1"), "first"));
    expectFirst(); // 模拟 Lua 后立即崩溃:没有 hook / Kafka,完整补投字节已原子留存。
    EXPECT_EQ(1LL, save("5", version("5", "1"), "first"));
    EXPECT_EQ(-1LL, save("5", version("5", "1"), "different-parent"));
    for (int changed = 0; changed < 3; ++changed)
    {
        auto conflicting = *intent;
        if (changed == 0) conflicting.topic = "db_task_zone_1_g2";
        if (changed == 1) conflicting.player_database_task += "different";
        if (changed == 2) conflicting.player_database_1_task += "different";
        EXPECT_EQ(-1LL, saveIntent("5", version("5", "1"), "first", conflicting));
        expectFirst();
    }
    for (int missing = 0; missing < 3; ++missing)
    {
        auto incomplete = *intent;
        if (missing == 0) incomplete.topic.clear();
        if (missing == 1) incomplete.player_database_task.clear();
        if (missing == 2) incomplete.player_database_1_task.clear();
        EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "empty-outbox", incomplete));
        expectFirst();
    }
    auto badRoute = *intent;
    badRoute.topic = "db_task_zone_2";
    EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "wrong-zone", badRoute));
    badRoute.topic = "db_task_zone_1_g1";
    EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "noncanonical-generation", badRoute));
    badRoute.index_key = "player:save_pending:1:64";
    badRoute.topic = intent->topic;
    EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "wrong-bucket", badRoute));
    badRoute.index_key = "player:save_pending:1000000:" + std::to_string(testPlayerId % RedisSaveCommitIntent::kIndexBuckets);
    badRoute.topic = "db_task_zone_1000000";
    EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "unsupported-storage-zone", badRoute));
    expectFirst();
    // 索引错误类型必须先于父 SET 被发现;用专用错误索引键,不破坏真实分桶其他成员。
    const std::string wrongIndex = "player:save_pending:4294967295:" + std::to_string(testPlayerId % RedisSaveCommitIntent::kIndexBuckets);
    ASSERT_EQ(0LL, sync.Exists(wrongIndex)) << "专用错误类型夹具键已被占用";
    sync.Set(wrongIndex, "blocked");
    auto wrongType = *intent;
    wrongType.index_key = wrongIndex;
    wrongType.topic = "db_task_zone_4294967295";
    EXPECT_EQ(-1LL, saveIntent("5", version("5", "2"), "wrong-index-type", wrongType));
    EXPECT_EQ("blocked", sync.Get(wrongIndex));
    sync.Del(wrongIndex);
    expectFirst();
    const auto aliasVersion = version("5", "2");
    // 参数个数必须与脚本一致(11 个 ARGV):个数不对会先撞 'invalid argument count',用例照样拿到 ERROR 而不再检验别名。
    Reply alias(static_cast<redisReply*>(redisCommand(sync.context.get(), "EVAL %s 4 %b %b %b %b ignored 5 %b %b %b %b %b %b %b %b %b",
        kSaveWithCommitBarrierLuaScript, parentKey.data(), parentKey.size(), ownerKey.data(), ownerKey.size(),
        parentKey.data(), parentKey.size(), indexKey.data(), indexKey.size(), aliasVersion.data(), aliasVersion.size(),
        intent->topic.data(), intent->topic.size(), intent->player_database_task.data(), intent->player_database_task.size(),
        intent->player_database_1_task.data(), intent->player_database_1_task.size(), member.data(), member.size(),
        schemaGen.data(), schemaGen.size(), schemaGen.data(), schemaGen.size(), schemaMode.data(), schemaMode.size(),
        schemaWriter.data(), schemaWriter.size())), &freeReplyObject);
    ASSERT_NE(nullptr, alias);
    ASSERT_EQ(REDIS_REPLY_ERROR, alias->type);
    EXPECT_NE(std::string::npos, std::string(alias->str, alias->len).find("SAVE_BARRIER keys must differ"))
        << std::string(alias->str, alias->len);
    expectFirst();
    sync.Del(parentKey);
    EXPECT_EQ(-1LL, save("5", version("5", "1"), "first")) << "同版不能从意图猜出缺失父键";
    EXPECT_EQ(0LL, sync.Exists(parentKey));
    EXPECT_EQ(version("5", "1"), pending());
    sync.Set(parentKey, "first");
    // 不完整和错版的历史意图也不得被新版本静默覆盖。
    Reply partial(static_cast<redisReply*>(redisCommand(sync.context.get(), "HDEL %b outbox_player_database_1",
        barrierKey.data(), barrierKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, partial);
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "partial-stored-outbox"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ("(nil)", field("outbox_player_database_1"));
    setPending(version("5", "1"));
    EXPECT_EQ(1LL, save("5", version("5", "1"), "first")) << "完成清理后的同版原字节可重新登记意图";
    expectFirst();
    sync.Set(ownerKey, "6");
    EXPECT_EQ(0LL, save("5", version("5", "2"), "deposed"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ(version("5", "1"), pending());
    sync.Del(ownerKey);
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "missing-owner"));
    EXPECT_EQ(0LL, sync.Exists(ownerKey)) << "严格路径不能自动补种旧 owner";
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ(version("5", "1"), pending());

    Reply ownerList(static_cast<redisReply*>(redisCommand(sync.context.get(), "LPUSH %b blocked", ownerKey.data(), ownerKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, ownerList);
    ASSERT_EQ(REDIS_REPLY_INTEGER, ownerList->type);
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "wrong-owner-type"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ(version("5", "1"), pending());
    sync.Del(ownerKey);
    sync.Set(ownerKey, "5");
    sync.Del(barrierKey);
    sync.Set(barrierKey, "wrong-type");
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "partial-parent"));
    EXPECT_EQ("first", sync.Get(parentKey)) << "HSET 类型错误前必须先校验,不得已 SET 父键";
    EXPECT_EQ("wrong-type", sync.Get(barrierKey));
    sync.Del(barrierKey);
    setPending("bad-version");
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "bad-pending"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ("bad-version", pending());
    setPending("00000000000000000005:18446744073709551616");
    EXPECT_EQ(-1LL, save("5", version("5", "2"), "overflow-pending"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ("00000000000000000005:18446744073709551616", pending());

    setPending(version("5", "9007199254740993"));
    EXPECT_EQ(-1LL, save("5", version("5", "9007199254740992"), "rounded-backwards"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ(version("5", "9007199254740993"), pending());
    EXPECT_EQ(-1LL, save("5", version("6", "1"), "owner-token-mismatch"));
    EXPECT_EQ(-1LL, save("5", "00000000000000000005:18446744073709551616", "overflow-seq"));
    EXPECT_EQ(-1LL, save("5", "18446744073709551616:00000000000000000001", "overflow-owner"));
    EXPECT_EQ("first", sync.Get(parentKey));
    EXPECT_EQ(version("5", "9007199254740993"), pending());

    sync.Del(parentKey);
    Reply list(static_cast<redisReply*>(redisCommand(sync.context.get(), "LPUSH %b blocked", parentKey.data(), parentKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(REDIS_REPLY_INTEGER, list->type);
    EXPECT_EQ(-1LL, save("5", version("5", "9007199254740994"), "wrong-parent-type"));
    EXPECT_EQ(version("5", "9007199254740993"), pending());
    sync.Del(parentKey);
    sync.Set(ownerKey, "18446744073709551615");
    EXPECT_EQ(1LL, save("18446744073709551615", version("18446744073709551615", "18446744073709551615"), "uint64-max"));
    EXPECT_EQ("uint64-max", sync.Get(parentKey));
    EXPECT_EQ(version("18446744073709551615", "18446744073709551615"), pending());
    EXPECT_EQ(version("18446744073709551615", "18446744073709551615"), field("outbox_version"));
    EXPECT_EQ(intent->player_database_task, field("outbox_player_database"));
    EXPECT_EQ(intent->player_database_1_task, field("outbox_player_database_1"));
    EXPECT_EQ("0", indexScore());
}

// 覆盖世代校验 C3(docs/design/mixed-version-persist-field-protection.md §5.2、§10)的真 Redis 语义:
// 只在"上次屏障写不是本实体写的"时比较;同一实体连续存盘、换 owner 代次后存盘都不误报(S-A 回归);
// warn 照写返回 2、enforce 一个键都不动返回 3;hash 只升不降;存储值损坏按倒退;同版本重放跳过;参数错误报错。
TEST(CurrencyTest, RedisCommitBarrierSchemaGenAgainstRealRedis)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port)) GTEST_SKIP() << "需要专用 MMORPG_TEST_REDIS_ADDR";
    SyncRedisForTest sync;
    ASSERT_TRUE(sync.Connect(host, port));
    const Guid testPlayerId = IsolatedRedisPlayerIdForTest();
    const std::string member = std::to_string(testPlayerId);
    const std::string parentKey = "CurrencyComp:" + member;
    const std::string ownerKey = "player:" + member + ":owner_epoch";
    const std::string barrierKey = "player:{" + member + "}:save_barrier";
    const auto intent = CommitIntentForTest(testPlayerId);
    const std::string& indexKey = intent->index_key;
    ASSERT_EQ(0LL, sync.Exists(parentKey));
    ASSERT_EQ(0LL, sync.Exists(ownerKey));
    ASSERT_EQ(0LL, sync.Exists(barrierKey));
    struct Cleanup
    {
        SyncRedisForTest& sync;
        const std::string& parent;
        const std::string& owner;
        const std::string& barrier;
        const std::string& index;
        const std::string& member;
        ~Cleanup()
        {
            sync.Del(parent); sync.Del(owner); sync.Del(barrier);
            freeReplyObject(redisCommand(sync.context.get(), "ZREM %b %b", index.data(), index.size(), member.data(), member.size()));
        }
    } cleanup{sync, parentKey, ownerKey, barrierKey, indexKey, member};
    using Reply = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;
    const auto version = [](const std::string& owner, const std::string& seq) {
        return std::string(20 - owner.size(), '0') + owner + ":" + std::string(20 - seq.size(), '0') + seq;
    };
    const auto field = [&](const std::string& name) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HGET %b %b",
            barrierKey.data(), barrierKey.size(), name.data(), name.size())), &freeReplyObject);
        return reply != nullptr && reply->type == REDIS_REPLY_STRING ? std::string(reply->str, reply->len) : "(nil)";
    };
    const auto setField = [&](const std::string& name, const std::string& value) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HSET %b %b %b",
            barrierKey.data(), barrierKey.size(), name.data(), name.size(), value.data(), value.size())), &freeReplyObject);
        ASSERT_NE(nullptr, reply);
        ASSERT_EQ(REDIS_REPLY_INTEGER, reply->type);
    };
    const auto delField = [&](const std::string& name) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HDEL %b %b",
            barrierKey.data(), barrierKey.size(), name.data(), name.size())), &freeReplyObject);
        ASSERT_NE(nullptr, reply);
    };
    std::string lastError;
    // 返回脚本的整数;ERROR 返回 -1(错误文案留在 lastError),其余异常返回 -99。
    const auto save = [&](const std::string& owner, const std::string& token, const std::string& body,
        const std::string& outputGen, const std::string& loadGen, const std::string& mode, const std::string& writer) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "EVAL %s 4 %b %b %b %b %b %b %b %b %b %b %b %b %b %b %b",
            kSaveWithCommitBarrierLuaScript, parentKey.data(), parentKey.size(), ownerKey.data(), ownerKey.size(),
            barrierKey.data(), barrierKey.size(), indexKey.data(), indexKey.size(),
            body.data(), body.size(), owner.data(), owner.size(), token.data(), token.size(),
            intent->topic.data(), intent->topic.size(), intent->player_database_task.data(), intent->player_database_task.size(),
            intent->player_database_1_task.data(), intent->player_database_1_task.size(), member.data(), member.size(),
            outputGen.data(), outputGen.size(), loadGen.data(), loadGen.size(), mode.data(), mode.size(),
            writer.data(), writer.size())), &freeReplyObject);
        lastError.clear();
        if (reply == nullptr) return -99LL;
        if (reply->type == REDIS_REPLY_ERROR)
        {
            lastError.assign(reply->str, reply->len);
            return -1LL;
        }
        return reply->type == REDIS_REPLY_INTEGER ? reply->integer : -99LL;
    };
    const std::string writerA = "3f2504e0-4f89-41d3-9a0c-0305e82c3301";
    const std::string writerB = "writer:b";
    const std::string writerC = "writer-c";

    sync.Set(ownerKey, "5");
    // 1. hash 不存在 = 不比较:新版二进制(世代 6)接手戳为 5 的数据,第一次写入通过,hash 记下 6 与令牌。
    ASSERT_EQ(1LL, save("5", version("5", "1"), "a1", "6", "5", "enforce", writerA));
    EXPECT_EQ("6", field("schema_gen"));
    EXPECT_EQ(writerA, field("schema_writer"));
    // 2. S-A 回归:同一实体连续存盘,加载世代仍是 5 < 6,令牌相同不比较。
    EXPECT_EQ(1LL, save("5", version("5", "2"), "a2", "6", "5", "enforce", writerA));
    EXPECT_EQ("a2", sync.Get(parentKey));
    // 3. 同一实体换 owner 代次(E→E+1 复用实体 / B5 采纳)后存盘:令牌相同,同样不比较。
    sync.Set(ownerKey, "6");
    EXPECT_EQ(1LL, save("6", version("6", "3"), "a3", "6", "5", "enforce", writerA));
    EXPECT_EQ("6", field("schema_gen"));
    // 4. 别的实体接手(令牌不同)且加载世代更低:warn 照写、返回 2,hash 不降,令牌换成它的。
    EXPECT_EQ(2LL, save("6", version("6", "4"), "b4", "6", "5", "warn", writerB));
    EXPECT_EQ("b4", sync.Get(parentKey));
    EXPECT_EQ(version("6", "4"), field("pending"));
    EXPECT_EQ("6", field("schema_gen"));
    EXPECT_EQ(writerB, field("schema_writer"));
    // 5. enforce:返回 3,一个键都不动(父快照、pending、意图、令牌、世代都保持第 4 步的值),下一次仍比较、仍拒。
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        EXPECT_EQ(3LL, save("6", version("6", "5"), "c5", "6", "5", "enforce", writerC));
        EXPECT_EQ("b4", sync.Get(parentKey));
        EXPECT_EQ(version("6", "4"), field("pending"));
        EXPECT_EQ(version("6", "4"), field("outbox_version"));
        EXPECT_EQ(writerB, field("schema_writer"));
        EXPECT_EQ("6", field("schema_gen"));
    }
    // 6. 已被废黜的旧持有者先拿 0,不能先拿 3(否则不会被 DestroyDeposedPlayer 销毁)。
    EXPECT_EQ(0LL, save("5", version("5", "9"), "deposed", "6", "5", "enforce", writerC));
    // 7. 加载世代不低于记录值:通过。
    EXPECT_EQ(1LL, save("6", version("6", "6"), "c6", "6", "6", "enforce", writerC));
    // 8. 只升不降:低世代写方(输出 3)warn 照写,hash 仍是 6。
    EXPECT_EQ(2LL, save("6", version("6", "7"), "low", "3", "3", "warn", writerA));
    EXPECT_EQ("6", field("schema_gen"));
    // 9. num_gt 边界(先比长度再比字节):10 对 9、9 对 10、19 对 1、相等。
    setField("schema_writer", "someone-else");
    setField("schema_gen", "10");
    EXPECT_EQ(3LL, save("6", version("6", "8"), "n1", "10", "9", "enforce", writerB)) << "9 < 10";
    setField("schema_gen", "9");
    EXPECT_EQ(1LL, save("6", version("6", "8"), "n2", "10", "10", "enforce", writerB)) << "10 > 9,不能按字节序判成更小";
    EXPECT_EQ("10", field("schema_gen"));
    setField("schema_writer", "someone-else");
    setField("schema_gen", "19");
    EXPECT_EQ(3LL, save("6", version("6", "9"), "n3", "1", "1", "enforce", writerB)) << "1 < 19";
    setField("schema_gen", "10");
    EXPECT_EQ(1LL, save("6", version("6", "9"), "n4", "10", "10", "enforce", writerB)) << "相等通过";
    // 10. 存储值损坏 = 倒退证据(不报错,否则 OnSaved 无限重试):enforce 拒;warn 照写并改成 ARGV[8]。
    for (const std::string corrupt : {"abc", "0", "01", "4294967296", ""})
    {
        setField("schema_writer", "someone-else");
        setField("schema_gen", corrupt);
        const std::string parentBefore = sync.Get(parentKey);
        const std::string pendingBefore = field("pending");
        EXPECT_EQ(3LL, save("6", version("6", "10"), "corrupt", "6", "6", "enforce", writerB)) << "schema_gen='" << corrupt << "'";
        EXPECT_EQ(parentBefore, sync.Get(parentKey));
        EXPECT_EQ(pendingBefore, field("pending"));
        EXPECT_EQ(corrupt, field("schema_gen"));
        EXPECT_EQ(2LL, save("6", version("6", "10"), "corrupt-warn", "6", "6", "warn", writerB)) << "schema_gen='" << corrupt << "'";
        EXPECT_EQ("6", field("schema_gen"));
        // 下一轮复用同一版本号:把 pending 退回上一步之前的值,模拟"上一轮没写过"。
        setField("pending", pendingBefore);
        setField("outbox_version", pendingBefore);
    }
    // 11. 字段不存在 = 不比较,写入 ARGV[8]。
    delField("schema_gen");
    setField("schema_writer", "someone-else");
    EXPECT_EQ(1LL, save("6", version("6", "11"), "missing", "2", "1", "enforce", writerC));
    EXPECT_EQ("2", field("schema_gen"));
    EXPECT_EQ(writerC, field("schema_writer"));
    // 12. 同版本重放(首存已提交、回包丢失后重发同一元素)跳过 C3:即使令牌被换掉、记录值更高也返回 1。
    setField("schema_writer", "someone-else");
    setField("schema_gen", "9");
    EXPECT_EQ(1LL, save("6", version("6", "11"), "missing", "2", "1", "enforce", writerC));
    EXPECT_EQ("9", field("schema_gen")) << "重放照写,但 hash 只升不降";
    // 13. 参数错误报错,且不改任何键。
    const std::string parentBefore = sync.Get(parentKey);
    const std::string pendingBefore = field("pending");
    const std::string tooLongWriter(65, 'w');
    const std::vector<std::vector<std::string>> badArgs{
        {"0", "1", "warn", writerA}, {"1", "0", "warn", writerA}, {"6", "7", "warn", writerA},
        {"4294967296", "1", "warn", writerA}, {"06", "1", "warn", writerA}, {"6", "1", "Warn", writerA},
        {"6", "1", "", writerA}, {"6", "1", "warn", ""}, {"6", "1", "warn", tooLongWriter}, {"6", "1", "warn", "bad writer"},
    };
    for (const auto& args : badArgs)
    {
        EXPECT_EQ(-1LL, save("6", version("6", "12"), "bad-args", args[0], args[1], args[2], args[3]))
            << args[0] << "/" << args[1] << "/" << args[2] << "/" << args[3];
        EXPECT_NE(std::string::npos, lastError.find("SAVE_BARRIER invalid schema argument")) << lastError;
        EXPECT_EQ(parentBefore, sync.Get(parentKey));
        EXPECT_EQ(pendingBefore, field("pending"));
    }
}

TEST(CurrencyTest, TravelSqlBarrierWaitsForBothSqlAcksAgainstRealRedis)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port)) GTEST_SKIP() << "需要专用 MMORPG_TEST_REDIS_ADDR";
    SyncRedisForTest sync;
    ASSERT_TRUE(sync.Connect(host, port));
    const auto member = std::to_string(IsolatedRedisPlayerIdForTest());
    const std::string ownerKey = "player:" + member + ":owner_epoch";
    const std::string barrierKey = "player:{" + member + "}:save_barrier";
    const std::string markKey = "player:" + member + ":handoff";
    ASSERT_EQ(0LL, sync.Exists(ownerKey));
    ASSERT_EQ(0LL, sync.Exists(barrierKey));
    ASSERT_EQ(0LL, sync.Exists(markKey));
    struct Cleanup
    {
        SyncRedisForTest& sync;
        const std::string& owner;
        const std::string& barrier;
        const std::string& mark;
        ~Cleanup() { sync.Del(owner); sync.Del(barrier); sync.Del(mark); }
    } cleanup{sync, ownerKey, barrierKey, markKey};
    sync.Del(ownerKey); sync.Del(barrierKey); sync.Del(markKey);
    using Reply = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;
    const std::string version = "00000000000000000005:00009007199254740993";
    const std::string newer = "00000000000000000005:00009007199254740994";
    const auto setField = [&](const std::string& name, const std::string& value) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "HSET %b %b %b",
            barrierKey.data(), barrierKey.size(), name.data(), name.size(), value.data(), value.size())), &freeReplyObject);
        ASSERT_NE(nullptr, reply);
        ASSERT_EQ(REDIS_REPLY_INTEGER, reply->type);
    };
    const auto publish = [&](const std::string& marker) {
        Reply reply(static_cast<redisReply*>(redisCommand(sync.context.get(), "EVAL %s 3 %b %b %b 5 %b 300",
            handoff_sql_barrier::kPublishMarkLua, ownerKey.data(), ownerKey.size(), barrierKey.data(), barrierKey.size(),
            markKey.data(), markKey.size(), marker.data(), marker.size())), &freeReplyObject);
        if (reply == nullptr) return -99LL;
        if (reply->type == REDIS_REPLY_ERROR) return -2LL;
        return reply->type == REDIS_REPLY_INTEGER ? reply->integer : -99LL;
    };
    const std::string marker = "5:1000";
    sync.Set(ownerKey, "5");
    setField("pending", version);
    // 连续 0 -> 0 -> 1,只有双 SQL ACK 齐备的那一次可以建立标记,此前不得请求归属推进。
    EXPECT_EQ(0LL, publish(marker));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("player_database", version);
    EXPECT_EQ(0LL, publish(marker));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("player_database_1", version);
    EXPECT_EQ(1LL, publish(marker));
    EXPECT_EQ(marker, sync.Get(markKey));
    Reply alias(static_cast<redisReply*>(redisCommand(sync.context.get(), "EVAL %s 3 %b %b %b 5 %b 300",
        handoff_sql_barrier::kPublishMarkLua, ownerKey.data(), ownerKey.size(), barrierKey.data(), barrierKey.size(),
        barrierKey.data(), barrierKey.size(), marker.data(), marker.size())), &freeReplyObject);
    ASSERT_NE(nullptr, alias);
    EXPECT_EQ(REDIS_REPLY_ERROR, alias->type);
    EXPECT_EQ(marker, sync.Get(markKey));
    Reply ttl(static_cast<redisReply*>(redisCommand(sync.context.get(), "TTL %b", markKey.data(), markKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, ttl);
    ASSERT_EQ(REDIS_REPLY_INTEGER, ttl->type);
    EXPECT_GT(ttl->integer, 0);
    EXPECT_LE(ttl->integer, 300);
    sync.Del(markKey);
    setField("pending", newer);
    EXPECT_EQ(0LL, publish("5:1001")) << "旧双 ACK 不能释放更新快照";
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("player_database", newer);
    EXPECT_EQ(0LL, publish("5:1001"));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("player_database_1", newer);
    EXPECT_EQ(1LL, publish("5:1001"));
    EXPECT_EQ("5:1001", sync.Get(markKey));
    sync.Del(markKey);

    sync.Set(ownerKey, "6");
    EXPECT_EQ(-1LL, publish("5:1002"));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    sync.Set(ownerKey, "5");
    setField("pending", "00000000000000000006:00000000000000000001");
    EXPECT_EQ(-2LL, publish("5:1002"));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("pending", newer);
    setField("player_database", "00000000000000000005:18446744073709551616");
    EXPECT_EQ(-2LL, publish("5:1002"));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    setField("player_database", newer);
    EXPECT_EQ(-2LL, publish("6:1002"));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    sync.Del(ownerKey);
    EXPECT_EQ(-2LL, publish("5:1002"));
    EXPECT_EQ(0LL, sync.Exists(ownerKey)) << "门禁不补种归属";
    sync.Set(ownerKey, "5");
    sync.Del(barrierKey);
    sync.Set(barrierKey, "blocked");
    EXPECT_EQ(-2LL, publish("5:1002"));
    EXPECT_EQ("blocked", sync.Get(barrierKey));
    EXPECT_EQ(0LL, sync.Exists(markKey));
    sync.Del(barrierKey);
    setField("pending", newer); setField("player_database", newer); setField("player_database_1", newer);
    Reply wrongType(static_cast<redisReply*>(redisCommand(sync.context.get(), "LPUSH %b blocked", markKey.data(), markKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, wrongType);
    ASSERT_EQ(REDIS_REPLY_INTEGER, wrongType->type);
    EXPECT_EQ(-2LL, publish("5:1002"));
    Reply listValue(static_cast<redisReply*>(redisCommand(sync.context.get(), "LINDEX %b 0", markKey.data(), markKey.size())), &freeReplyObject);
    ASSERT_NE(nullptr, listValue);
    ASSERT_EQ(REDIS_REPLY_STRING, listValue->type);
    EXPECT_EQ("blocked", std::string(listValue->str, listValue->len));
}

// Q3(颠倒的回归用例):已连接、无在途、排队里有一份失败后等退避的旧值时,新值不得被直接发出。
// 修复前这条红:新值被 IssueSave 发出(在途 1),排队里仍是更旧的 S1。只需要"已连接"这一状态,不等回包。
TEST(CurrencyTest, RedisConnectedSaveDoesNotBypassFailedPending)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port))
    {
        GTEST_SKIP() << "MMORPG_TEST_REDIS_ADDR 未设置(host:port),跳过真 Redis 用例";
    }

    using Client = MessageAsyncClient<Guid, CurrencyComp>;
    using Peer = MessageAsyncClientTestPeer<Guid, CurrencyComp>;

    muduo::net::EventLoop loop;
    Client::HiredisPtr connection = ConnectForTest(loop, host, port);
    const LeakConnectionOnExit leak{connection};
    ASSERT_TRUE(connection->connected()) << "连不上 " << host << ":" << port;
    {
        Client client(connection);
        constexpr Guid kPlayerId = 950299001;
        const auto s1 = Peer::TrackSave(client, kPlayerId, MarkedCurrency(1));
        auto error = ErrorReply();
        Peer::DeliverSaved(client, &error, s1);
        ASSERT_EQ(s1, Peer::PendingSave(client, kPlayerId));

        client.Save(MarkedCurrency(2), kPlayerId);

        EXPECT_EQ(0u, client.in_flight_save_count()) << "新值不得绕过排队的旧值直接发出";
        const auto pending = Peer::PendingSave(client, kPlayerId);
        ASSERT_NE(nullptr, pending);
        EXPECT_EQ(2u, MarkerOf(*pending->message_value)) << "排队的是新值,旧值 S1 已被顶替";
    }
}

// Q4(守卫脚本语义,端到端):guard 相等 → 写入;不等 → 拒绝且数据不变;缺键 → 补种并写入。
TEST(CurrencyTest, RedisGuardedSaveSemanticsAgainstRealRedis)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port))
    {
        GTEST_SKIP() << "MMORPG_TEST_REDIS_ADDR 未设置(host:port),跳过真 Redis 用例";
    }

    using Client = MessageAsyncClient<Guid, CurrencyComp>;

    SyncRedisForTest sync;
    ASSERT_TRUE(sync.Connect(host, port)) << "同步连接失败 " << host << ":" << port;

    muduo::net::EventLoop loop;
    Client::HiredisPtr connection = ConnectForTest(loop, host, port);
    const LeakConnectionOnExit leak{connection};
    ASSERT_TRUE(connection->connected()) << "连不上 " << host << ":" << port;
    {
        Client client(connection);
        std::string outcome;
        client.SetSaveCallback([&](Guid, CurrencyComp&) {
            outcome = "saved";
            loop.quit();
        });
        client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) {
            outcome = "rejected";
            loop.quit();
        });

        constexpr Guid kPlayerId = 950299002;
        const std::string guardKey = "b4c_test:950299002:owner_epoch";
        const std::string blobKey = client.full_name() + ":" + std::to_string(kPlayerId);
        const auto cleanup = [&] {
            sync.Del(blobKey);
            sync.Del(guardKey);
            sync.SRem("dirty_keys_set", blobKey);
        };
        // 一次完整的带 guard 存盘:跑事件循环直到落地回调或 3 秒超时,返回结局("" = 超时)。
        const auto saveOnce = [&](const std::shared_ptr<CurrencyComp>& value, const std::string& expected) {
            outcome.clear();
            client.Save(value, kPlayerId, guardKey, expected);
            const auto timeout = loop.runAfter(3.0, [&loop] { loop.quit(); });
            loop.loop();
            loop.cancel(timeout);
            return outcome;
        };
        cleanup();

        // 1. guard 相等 → 写入
        sync.Set(guardKey, "5");
        const auto first = MarkedCurrency(1);
        EXPECT_EQ("saved", saveOnce(first, "5"));
        EXPECT_EQ(first->SerializeAsString(), sync.Get(blobKey));

        // 2. guard 不等 → 拒绝,数据不变
        sync.Set(guardKey, "6");
        EXPECT_EQ("rejected", saveOnce(MarkedCurrency(2), "5"));
        EXPECT_EQ(first->SerializeAsString(), sync.Get(blobKey)) << "被拒的写不能落地";

        // 3. guard 缺键 → 补种成调用方的期望值,并写入
        sync.Del(guardKey);
        const auto third = MarkedCurrency(3);
        EXPECT_EQ("saved", saveOnce(third, "9"));
        EXPECT_EQ(third->SerializeAsString(), sync.Get(blobKey));
        EXPECT_EQ("9", sync.Get(guardKey));

        cleanup();
    }
}

// A0-4 回归(端到端):普通存盘与带 guard 存盘都只写数据键(与 guard 键),
// 不再把数据键登记进旧版写死的全服集合。修复前这条红:两段脚本都会 SADD 它。
// 键级断言只在用例开始时库里没有这个集合时才做:专用测试 Redis 若留有旧二进制写下的集合,
// 那些成员不归本用例管,此时只按本用例自己的成员判定,结果不依赖外部残留。
TEST(CurrencyTest, RedisSaveLeavesNoGlobalDirtySetAgainstRealRedis)
{
    std::string host;
    uint16_t port = 0;
    if (!TestRedisAddr(host, port))
    {
        GTEST_SKIP() << "MMORPG_TEST_REDIS_ADDR 未设置(host:port),跳过真 Redis 用例";
    }

    using Client = MessageAsyncClient<Guid, CurrencyComp>;

    SyncRedisForTest sync;
    ASSERT_TRUE(sync.Connect(host, port)) << "同步连接失败 " << host << ":" << port;

    muduo::net::EventLoop loop;
    Client::HiredisPtr connection = ConnectForTest(loop, host, port);
    const LeakConnectionOnExit leak{connection};
    ASSERT_TRUE(connection->connected()) << "连不上 " << host << ":" << port;
    {
        Client client(connection);
        std::string outcome;
        client.SetSaveCallback([&](Guid, CurrencyComp&) {
            outcome = "saved";
            loop.quit();
        });
        client.SetSaveRejectedCallback([&](Guid, const std::string&, const std::string&) {
            outcome = "rejected";
            loop.quit();
        });

        constexpr Guid kPlayerId = 950299003;
        const std::string guardKey = "b4c_test:950299003:owner_epoch";
        const std::string blobKey = client.full_name() + ":" + std::to_string(kPlayerId);
        const std::string legacyDirtySet = "dirty_keys_set";
        const auto cleanup = [&] {
            sync.Del(blobKey);
            sync.Del(guardKey);
            sync.SRem(legacyDirtySet, blobKey);
        };
        // 跑事件循环直到落地回调或 3 秒超时,返回结局("" = 超时)。
        const auto settle = [&] {
            const auto timeout = loop.runAfter(3.0, [&loop] { loop.quit(); });
            loop.loop();
            loop.cancel(timeout);
            return outcome;
        };
        cleanup();
        const long long legacySetBefore = sync.Exists(legacyDirtySet);
        ASSERT_NE(-1LL, legacySetBefore) << "EXISTS 失败";

        // 1. 普通存盘(epoch==0 兼容窗口走这条)
        outcome.clear();
        const auto plain = MarkedCurrency(1);
        client.Save(plain, kPlayerId);
        EXPECT_EQ("saved", settle());
        EXPECT_EQ(plain->SerializeAsString(), sync.Get(blobKey));
        EXPECT_EQ(0LL, sync.SIsMember(legacyDirtySet, blobKey)) << "普通存盘不得登记进全服集合";

        // 2. 带 guard 存盘
        sync.Set(guardKey, "5");
        outcome.clear();
        const auto guarded = MarkedCurrency(2);
        client.Save(guarded, kPlayerId, guardKey, "5");
        EXPECT_EQ("saved", settle());
        EXPECT_EQ(guarded->SerializeAsString(), sync.Get(blobKey));
        EXPECT_EQ(0LL, sync.SIsMember(legacyDirtySet, blobKey)) << "带 guard 存盘不得登记进全服集合";

        if (legacySetBefore == 0)
        {
            EXPECT_EQ(0LL, sync.Exists(legacyDirtySet)) << "存盘后全服集合不应存在";
        }

        cleanup();
    }
}

// ---------------------------------------------------------------------------
// BlockCurrency (GM禁止获取)
// ---------------------------------------------------------------------------

TEST(CurrencyTest, BlockCurrencyPreventsAdd)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::BlockCurrency(player, kCurrencyGold));
    EXPECT_TRUE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    // AddCurrency should be rejected for blocked type.
    EXPECT_NE(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, BlockCurrencyDoesNotAffectDeduct)
{
    auto player = CreateTestPlayer();

    // Pre-load some gold first.
    CurrencySystem::AddCurrency(player, kCurrencyGold, 200);
    CurrencySystem::BlockCurrency(player, kCurrencyGold);

    // Deduct should still work even when blocked.
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 50));
    EXPECT_EQ(150u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, BlockDoesNotAffectOtherTypes)
{
    auto player = CreateTestPlayer();

    CurrencySystem::BlockCurrency(player, kCurrencyGold);

    // Diamond is not blocked.
    EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyDiamond));
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyDiamond, 100));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyDiamond));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, UnblockCurrencyAllowsAdd)
{
    auto player = CreateTestPlayer();

    CurrencySystem::BlockCurrency(player, kCurrencyGold);
    EXPECT_TRUE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    CurrencySystem::UnblockCurrency(player, kCurrencyGold);
    EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, BlockCurrencyIdempotent)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(kSuccess, CurrencySystem::BlockCurrency(player, kCurrencyGold));
    EXPECT_EQ(kSuccess, CurrencySystem::BlockCurrency(player, kCurrencyGold));
    EXPECT_TRUE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    // Unblock once should be enough.
    EXPECT_EQ(kSuccess, CurrencySystem::UnblockCurrency(player, kCurrencyGold));
    EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, UnblockNonBlockedIsNoOp)
{
    auto player = CreateTestPlayer();

    EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));
    EXPECT_EQ(kSuccess, CurrencySystem::UnblockCurrency(player, kCurrencyGold));
    EXPECT_FALSE(CurrencySystem::IsCurrencyBlocked(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// GetBalance edge cases
// ---------------------------------------------------------------------------

TEST(CurrencyTest, GetBalanceDefaultZero)
{
    auto player = CreateTestPlayer();

    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyDiamond));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyBindDiamond));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// 资产通道错误码细分(docs/design/guild-phase2/04-asset-channel.md §4.11、§4.14.3)
//
// 这几条断言的是**具体取值**,不是 `EXPECT_NE(kSuccess, …)`。上面那些老用例只验
// "失败了",于是 2026-09 之前"冻结"和"余额不足"和"参数写错了"回同一个
// kInvalidParameter 也一路绿着 —— 而通用资产通道要靠这个返回值区分
// RETRY(条件会消失,可重投)与 REJECTED(终局拒绝,记账并退款),混在一起
// 就是要么重复发放、要么把玩家过图那几百毫秒里的奖励永久丢掉。
// ---------------------------------------------------------------------------

TEST(CurrencyTest, DeductInsufficientReturnsAssetCode)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 10);
    EXPECT_EQ(kAssetCurrencyInsufficient, CurrencySystem::DeductCurrency(player, kCurrencyGold, 20));
    EXPECT_EQ(10u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DeductFrozenReturnsAssetFrozen)
{
    auto player = CreateTestPlayer();

    CurrencySystem::AddCurrency(player, kCurrencyGold, 100);
    // PlayerLifecycleSystem::IsCrossZoneFrozen 就是 valid() + any_of<PlayerFrozenComp>
    // (player_lifecycle.cpp,函数名定位;那个文件正被跨 zone 会话改着,行号写下来就过期)。
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(player);

    EXPECT_EQ(kAssetFrozen, CurrencySystem::DeductCurrency(player, kCurrencyGold, 30));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));

    // 解冻之后同一笔扣款必须成立 —— 这条才说明 kAssetFrozen 真的是"暂时"。
    tlsEcs.actorRegistry.remove<PlayerFrozenComp>(player);
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 30));
    EXPECT_EQ(70u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AddFrozenReturnsAssetFrozen)
{
    auto player = CreateTestPlayer();

    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(player);
    EXPECT_EQ(kAssetFrozen, CurrencySystem::AddCurrency(player, kCurrencyGold, 50));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    tlsEcs.actorRegistry.remove<PlayerFrozenComp>(player);
    EXPECT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 50));
    EXPECT_EQ(50u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, AddBlockedReturnsAssetBlocked)
{
    auto player = CreateTestPlayer();

    ASSERT_EQ(kSuccess, CurrencySystem::BlockCurrency(player, kCurrencyGold));
    EXPECT_EQ(kAssetBlocked, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));

    // 封禁只挡获取,不挡扣除(既有语义,顺带钉住:它不该回 kAssetBlocked)。
    CurrencySystem::UnblockCurrency(player, kCurrencyGold);
    ASSERT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    ASSERT_EQ(kSuccess, CurrencySystem::BlockCurrency(player, kCurrencyGold));
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 40));
    EXPECT_EQ(60u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, ProgrammingErrorsStillReturnInvalidParameter)
{
    auto player = CreateTestPlayer();

    // 这条是反向护栏:错误码细分**只**覆盖冻结 / 余额不足 / 封禁三种业务条件。
    // "数量 <= 0""币种越界"是调用方写错了,仍旧回 kInvalidParameter ——
    // 把它们也搬进 27000 段,资产通道就会把编程错误当成可重投的业务条件。
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyGold, 0));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyGold, -1));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::DeductCurrency(player, kCurrencyGold, 0));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::DeductCurrency(player, kCurrencyGold, -5));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyMax, 10));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::DeductCurrency(player, kCurrencyMax, 10));

    // **判定顺序**:冻结 × 越界币种这个组合必须仍旧回 kInvalidParameter。
    // 纯参数校验排在冻结之后的话,这里会拿到 RETRY 类的 kAssetFrozen,资产通道就会
    // 对一个"解冻之后照样失败"的请求一直重投到超时 —— 那是一条只在两个条件同时
    // 成立时才现形的缝,所以单独钉住。
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(player);
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyMax, 10));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::DeductCurrency(player, kCurrencyMax, 10));
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyGold, 0));
    // 同一个冻结玩家、合法币种合法数量 —— 这时才该是 kAssetFrozen,证明上面三条
    // 不是"冻结判定被整个删掉了"。
    EXPECT_EQ(kAssetFrozen, CurrencySystem::AddCurrency(player, kCurrencyGold, 10));
    tlsEcs.actorRegistry.remove<PlayerFrozenComp>(player);

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// 必需组件缺失必须 fail-closed(docs/design/ecs-entt-misuse-audit-20261015.md F025)
//
// PlayerCurrencyComp 只在玩家加载时挂上(欠款从 CurrencyComp.debts 装载)。修复前增减路径在
// 缺它时当场 get_or_emplace 一本空的继续记账:欠款被绕过、组件集合被悄悄改掉,
// 下一次存盘还会用空表覆盖盘上的欠款。修复后:拒绝(kInvalidParameter)、余额不动、不补建组件。
// ---------------------------------------------------------------------------

TEST(CurrencyTest, AddCurrencyMissingPlayerCurrencyCompFailsClosed)
{
    // 只有 CurrencyComp、没有 PlayerCurrencyComp:模拟加载不完整的实体。
    const auto player = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.emplace<CurrencyComp>(player);

    // 修复前这三条都红:返回 kSuccess、余额变成 100、组件被补建出来。
    EXPECT_EQ(kInvalidParameter, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerCurrencyComp>(player));

    DestroyTestPlayer(player);
}

TEST(CurrencyTest, DeductCurrencyMissingPlayerCurrencyCompFailsClosed)
{
    auto player = CreateTestPlayer();
    ASSERT_EQ(kSuccess, CurrencySystem::AddCurrency(player, kCurrencyGold, 100));
    tlsEcs.actorRegistry.remove<PlayerCurrencyComp>(player);

    // 修复前先扣钱、再补建组件:返回 kSuccess、余额 60、组件重新出现 —— 三条都红。
    EXPECT_EQ(kInvalidParameter, CurrencySystem::DeductCurrency(player, kCurrencyGold, 40));
    EXPECT_EQ(100u, CurrencySystem::GetBalance(player, kCurrencyGold));
    // ASSERT 而非 EXPECT:修复前组件已被补建,下面的 emplace 会触发 entt 断言把整个测试进程带崩。
    ASSERT_FALSE(tlsEcs.actorRegistry.any_of<PlayerCurrencyComp>(player));

    // 组件按加载路径补回来之后同一笔扣款照常成立:拒绝的原因确实只是缺组件。
    tlsEcs.actorRegistry.emplace<PlayerCurrencyComp>(player);
    EXPECT_EQ(kSuccess, CurrencySystem::DeductCurrency(player, kCurrencyGold, 40));
    EXPECT_EQ(60u, CurrencySystem::GetBalance(player, kCurrencyGold));

    DestroyTestPlayer(player);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // 资产通道物品用例必须使用真实配表，不因默认配置目录为空而全部跳过。
    if (!test_config::FindAndLoadTestConfig(argc, argv)) return 1;
    // 生产线程由 scene 的 ConfigureGuidSegmentClients 按配置启用 txlog 号段并经 DataService 领首段;
    // 单测没有那条启动链。给 txlog 种类装一个不回话的假传输并直接投一段大范围,
    // 避免交易流水因 tx_id 号段未就绪被 fail-closed 丢弃(那条路径由 bag_test 专门覆盖)。
    {
        auto &txlog = tlsGuidSegmentRegistry.Get(GuidKind::kTxLog);
        GuidSegmentClient::Options options;
        options.kindName = "txlog";
        options.initialStep = 1000000;
        const bool enabled = txlog.Enable(
            options, [](const std::string &, uint32_t) { return true; },
            [](GuidSegmentClient::TimerKind, double, std::function<void()>) {});
        if (!enabled)
        {
            return 1;
        }
        txlog.Warm();
        txlog.OnResponse(0, 1, uint64_t{1} << 50);
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
