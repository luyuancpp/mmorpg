// inflight_index_test —— InflightIndex(engine/core 的 utils/data_structures/inflight_index.h)纯逻辑单测
// (docs/design/muduo-rpc-channel.md §4.4、§15 A)。
//
// RpcChannel 的「恰好一次」只建立在这张表上:Extract / ExtractExpired / ExtractAll 谁把条目取出来,谁负责完成;
// 其余路径(迟到的回包、迟到的超时扫描、已经清空的断连)一律取不到、什么都不做。
// 时刻全部合成(InflightSteadyTime{} + n ms),不读时钟、不起线程。
//
// 与 rpc_controller_test.cpp 同一工程、同一个 main。gate 的 router_lane_test.cpp 里也有 InflightIndex 套件的用例,
// 那是另一个可执行文件(独立编译),用例名不会冲突。

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "utils/data_structures/inflight_index.h"

namespace
{

using std::chrono::milliseconds;

struct TestCall
{
    InflightSteadyTime expiresAt;
    std::string name;
};

InflightSteadyTime At(int ms)
{
    return InflightSteadyTime{} + milliseconds(ms);
}

using IdList = std::vector<InflightCallId>;

IdList IdsOf(const std::vector<std::pair<InflightCallId, TestCall>>& batch)
{
    IdList ids;
    ids.reserve(batch.size());
    for (const auto& entry : batch)
    {
        ids.push_back(entry.first);
    }
    return ids;
}

IdList SortedIdsOf(const std::vector<std::pair<InflightCallId, TestCall>>& batch)
{
    IdList ids = IdsOf(batch);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

TEST(InflightIndex, InsertRejectsTheReservedZeroIdAndDuplicates)
{
    InflightIndex<TestCall> index;
    EXPECT_FALSE(index.Insert(kNoInflightCallId, {At(100), "zero"})) << "0 表示「不受索引管理」(单向调用),不得登记";
    EXPECT_TRUE(index.Empty());

    EXPECT_TRUE(index.Insert(7, {At(200), "first"}));
    EXPECT_FALSE(index.Insert(7, {At(100), "duplicate"}));
    EXPECT_EQ(index.Size(), 1u);

    // 被拒的重复登记不得留下更早的截止时刻:否则原调用会在它自己的截止之前被判超时。
    EXPECT_TRUE(index.ExtractExpired(At(150)).empty());
    const auto kept = index.Extract(7);
    ASSERT_TRUE(kept.has_value());
    EXPECT_EQ(kept->name, "first") << "重复登记不得覆盖原调用";
    EXPECT_TRUE(index.Empty());
}

TEST(InflightIndex, ExtractClaimsACallExactlyOnce)
{
    InflightIndex<TestCall> index;
    ASSERT_TRUE(index.Insert(3, {At(100), "call"}));

    const auto first = index.Extract(3);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name, "call");

    // 回包、超时、断连三条完成路径只有第一个取得到;后来者取不到就什么都不做。
    EXPECT_FALSE(index.Extract(3).has_value()) << "同一调用被认领了两次";
    EXPECT_FALSE(index.Erase(3));
    EXPECT_TRUE(index.ExtractExpired(At(1000)).empty()) << "已认领的调用还留在过期堆里,弹出时必须跳过";
    EXPECT_TRUE(index.ExtractAll().empty());
    EXPECT_FALSE(index.Extract(4).has_value()) << "从未登记的号(对端回了未知 id)";
    EXPECT_TRUE(index.Empty());
}

TEST(InflightIndex, ExtractExpiredReturnsDeadlineOrderAndSkipsCompletedCalls)
{
    InflightIndex<TestCall> index;
    ASSERT_TRUE(index.Insert(1, {At(500), "e"}));
    ASSERT_TRUE(index.Insert(2, {At(100), "a"}));
    ASSERT_TRUE(index.Insert(3, {At(300), "c"}));
    ASSERT_TRUE(index.Insert(4, {At(200), "b"}));
    ASSERT_TRUE(index.Insert(5, {At(400), "d"}));
    ASSERT_TRUE(index.Extract(3).has_value());   // 3 已由回包完成

    EXPECT_TRUE(index.ExtractExpired(At(99)).empty());
    EXPECT_EQ(IdsOf(index.ExtractExpired(At(350))), (IdList{2, 4})) << "按截止先后取出,并跳过已完成的 3";
    EXPECT_TRUE(index.ExtractExpired(At(399)).empty()) << "已被上一轮扫描取走的调用不得再被取出";

    const auto rest = index.ExtractExpired(At(500));
    ASSERT_EQ(IdsOf(rest), (IdList{5, 1})) << "截止时刻等于 now 也算到期";
    EXPECT_EQ(rest[0].second.name, "d");
    EXPECT_EQ(rest[1].second.name, "e");
    EXPECT_TRUE(index.Empty());
}

// RpcChannel 的用法:截止时刻 = 发起时刻 + 本次调用的超时。不同超时混排时,到期顺序只看截止时刻,与发起顺序无关;
// 截止前被回包认领的调用不会再被超时扫描取到。
TEST(InflightIndex, MixedTimeoutsExpireByDeadlineNotByIssueOrder)
{
    struct IssuedCall
    {
        InflightCallId id;
        int issuedAtMs;
        int timeoutMs;
    };
    const IssuedCall issued[] = {
        {1, 0, 10000},   // 默认超时 10s → 10000
        {2, 10, 50},     // → 60
        {3, 20, 3000},   // 握手超时 3s → 3020
        {4, 30, 50},     // → 80,到期前回包
        {5, 40, 50},     // → 90
        {6, 40, 50},     // → 90,与 5 同一截止时刻
    };

    InflightIndex<TestCall> index;
    for (const auto& call : issued)
    {
        ASSERT_TRUE(index.Insert(call.id, {At(call.issuedAtMs + call.timeoutMs), ""}));
    }

    EXPECT_EQ(IdsOf(index.ExtractExpired(At(70))), (IdList{2}));
    ASSERT_TRUE(index.Extract(4).has_value());
    EXPECT_EQ(SortedIdsOf(index.ExtractExpired(At(90))), (IdList{5, 6}))
        << "同一截止时刻的调用一次全部取出(彼此顺序不作约定),已回包的 4 被跳过";
    EXPECT_TRUE(index.ExtractExpired(At(3019)).empty());
    EXPECT_EQ(IdsOf(index.ExtractExpired(At(3020))), (IdList{3}));
    EXPECT_EQ(index.Size(), 1u);
    EXPECT_EQ(IdsOf(index.ExtractExpired(At(10000))), (IdList{1}));
    EXPECT_TRUE(index.Empty());
}

// 断连 / 通道关闭:整表取出,之后表与过期堆都是空的,批量取走的调用不能再被迟到的回包或超时扫描认领。
TEST(InflightIndex, ExtractAllEmptiesTheIndexAndTheExpiryHeap)
{
    InflightIndex<TestCall> index;
    ASSERT_TRUE(index.Insert(1, {At(100), "a"}));
    ASSERT_TRUE(index.Insert(2, {At(200), "b"}));
    ASSERT_TRUE(index.Insert(3, {At(300), "c"}));
    ASSERT_TRUE(index.Extract(2).has_value());

    EXPECT_EQ(SortedIdsOf(index.ExtractAll()), (IdList{1, 3})) << "返回顺序不保证,只比较集合";
    EXPECT_TRUE(index.Empty());
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_EQ(index.ExpiryHeapSize(), 0u) << "ExtractAll 必须连过期堆一起清空,否则惰性残留会随断连次数累积";
    EXPECT_TRUE(index.ExtractExpired(At(1000)).empty());
    EXPECT_FALSE(index.Extract(1).has_value()) << "批量取走的调用不得再被迟到的回包认领";
    EXPECT_TRUE(index.ExtractAll().empty());

    // 清空后照常可用(RpcChannel 的调用号单调递增、不复用,这里只验证表本身)。
    ASSERT_TRUE(index.Insert(4, {At(50), "after"}));
    EXPECT_EQ(IdsOf(index.ExtractExpired(At(50))), (IdList{4}));
    EXPECT_TRUE(index.Empty());
}

// 微基准(默认不跑):登记 + 认领一次的开销,载荷与 RpcChannel::OutstandingCall 同样带一个 std::function。
// 由 Codex 用 --gtest_also_run_disabled_tests 在改动前后各跑一次出对比表(AGENTS §6:没有对比表不声称性能结论)。
TEST(DISABLED_InflightIndexBench, InsertThenExtractNsPerOp)
{
    struct BenchCall
    {
        InflightSteadyTime expiresAt;
        std::function<void()> done;
    };
    constexpr int kOps = 1'000'000;
    InflightIndex<BenchCall> index;
    int completions = 0;

    const auto start = std::chrono::steady_clock::now();
    for (int i = 1; i <= kOps; ++i)
    {
        const auto id = static_cast<InflightCallId>(i);
        index.Insert(id, {At(i), [&completions] { ++completions; }});
        if (auto call = index.Extract(id))
        {
            call->done();
        }
    }
    // 惰性过期堆里的残留由一次扫描清掉,计入总开销。
    (void)index.ExtractExpired(At(kOps + 1));
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const double nsPerOp = std::chrono::duration<double, std::nano>(elapsed).count() / kOps;
    std::printf("[InflightIndexBench] insert+extract ops=%d ns_per_op=%.1f\n", kOps, nsPerOp);
    RecordProperty("ns_per_op", static_cast<int>(nsPerOp));
    EXPECT_EQ(completions, kOps);
    EXPECT_TRUE(index.Empty());
    EXPECT_EQ(index.ExpiryHeapSize(), 0u);
}
