// 第 2 批 etcd 新传输的纯函数部分(docs/design/grpc-muduo-transport.md §10.2、§13.5、§13.6)。
//
// 被测的三个头文件只依赖标准库(发现键索引 / 地址解析 / 流重建退避),放在本测试工程里随 muduo_grpc 一起跑,
// 不必为它们链 core 与官方 gRPC。EtcdService 上的整合行为(重建后重新 Range、对账删除走 HandleServiceNodeStop)
// 依赖 Node 全局状态,由 PROGRESS 条目里的 etcd 故障注入验证覆盖。

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/node/system/discovery/discovery_key_index.h"
#include "core/node/system/etcd/etcd_endpoint.h"
#include "core/node/system/etcd/etcd_stream_backoff.h"

namespace
{

using namespace std::chrono_literals;
using DeleteResult = DiscoveryKeyIndex::DeleteResult;

constexpr char kPrefix[] = "SceneNodeService.rpc";
constexpr char kKeyA[] = "SceneNodeService.rpc/zone/1/node_type/2/node_id/1";
constexpr char kKeyB[] = "SceneNodeService.rpc/zone/1/node_type/2/node_id/2";
constexpr char kOtherPrefixKey[] = "SceneManagerNodeService.rpc/zone/1/node_type/9/node_id/1";

std::vector<std::string> Keys(const std::vector<DiscoveryKeyIndex::Removed> &removed)
{
    std::vector<std::string> keys;
    for (const auto &entry : removed)
    {
        keys.push_back(entry.key);
    }
    return keys;
}

// ── 发现键索引 ──────────────────────────────────────────────────────────

TEST(DiscoveryKeyIndexTest, DeleteWithoutPrevKvReturnsTheLastKnownValue)
{
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "{\"v\":1}", 10));
    EXPECT_TRUE(index.ApplyPut(kKeyA, "{\"v\":2}", 12));

    const auto outcome = index.ApplyDelete(kKeyA, 15);
    EXPECT_EQ(outcome.result, DeleteResult::kRemoved);
    EXPECT_EQ(outcome.value, "{\"v\":2}") << "压缩后拿不到 prev_kv 时按 key 取回删除前的值(§10.2 第 4 条)";
    EXPECT_EQ(index.FindValue(kKeyA), nullptr);
    EXPECT_EQ(index.ApplyDelete(kKeyA, 16).result, DeleteResult::kUnknownKey);
}

TEST(DiscoveryKeyIndexTest, RepeatedSnapshotOfTheSameVersionIsAccepted)
{
    // 每 2s 的全量 Range 会反复带回同一版本:必须照样放行(发现层靠它在服务就绪后补建连)。
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v", 10));
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v", 10));
    EXPECT_EQ(index.size(), 1u);
}

TEST(DiscoveryKeyIndexTest, OlderPutIsStale)
{
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "new", 20));
    EXPECT_FALSE(index.ApplyPut(kKeyA, "old", 19)) << "Range 快照比已处理的 watch PUT 旧";
    ASSERT_NE(index.FindValue(kKeyA), nullptr);
    EXPECT_EQ(*index.FindValue(kKeyA), "new");
}

TEST(DiscoveryKeyIndexTest, PutOlderThanAProcessedDeleteIsStaleButARecreateIsNot)
{
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v1", 10));
    EXPECT_EQ(index.ApplyDelete(kKeyA, 20).result, DeleteResult::kRemoved);
    // 删除之前拍下的快照晚到:不得把已下线的节点加回来。
    EXPECT_FALSE(index.ApplyPut(kKeyA, "v1", 10));
    EXPECT_EQ(index.FindValue(kKeyA), nullptr);
    // 删除之后重新注册(更大的 mod_revision):照常接受,墓碑随之清掉。
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v2", 25));
    EXPECT_EQ(index.tombstoneCount(), 0u);
}

TEST(DiscoveryKeyIndexTest, DeleteOlderThanTheKnownPutIsStaleAndChangesNothing)
{
    // 快照(R ≥ 30)先于 watch 的旧 DELETE(20)到达:key 在 20 被删、30 又建了,旧删除必须忽略。
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "recreated", 30));
    const auto outcome = index.ApplyDelete(kKeyA, 20);
    EXPECT_EQ(outcome.result, DeleteResult::kStale);
    ASSERT_NE(index.FindValue(kKeyA), nullptr);
    EXPECT_EQ(*index.FindValue(kKeyA), "recreated");
}

TEST(DiscoveryKeyIndexTest, CompleteSnapshotRemovesKeysThatVanishedWhileTheStreamWasDown)
{
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyA, "a", 10);
    index.ApplyPut(kKeyB, "b", 11);
    index.ApplyPut(kOtherPrefixKey, "other", 12);

    // 流断期间 B 下线;重建后的快照(revision 50)里只有 A。
    const auto removed = index.ApplySnapshot(kPrefix, {kKeyA}, 50, /*complete=*/true);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0].key, kKeyB);
    EXPECT_EQ(removed[0].value, "b") << "对账删除要带上 NodeInfo,交给 HandleServiceNodeStop";
    EXPECT_NE(index.FindValue(kKeyA), nullptr);
    EXPECT_NE(index.FindValue(kOtherPrefixKey), nullptr) << "只动本前缀";
    // 对账删除记了墓碑:更旧的 PUT 晚到不会把它加回来。
    EXPECT_FALSE(index.ApplyPut(kKeyB, "b", 11));
}

TEST(DiscoveryKeyIndexTest, KeysNewerThanTheSnapshotSurviveReconcile)
{
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyA, "a", 10);
    index.ApplyPut(kKeyB, "b", 60); // 快照之后经 watch 才到
    const auto removed = index.ApplySnapshot(kPrefix, {kKeyA}, 50, true);
    EXPECT_TRUE(removed.empty());
    EXPECT_NE(index.FindValue(kKeyB), nullptr);
}

TEST(DiscoveryKeyIndexTest, IncompleteSnapshotNeverRemovesAnything)
{
    // more=true / 值解析失败 / key 不属于前缀:宁可多留,不错删(§10.2 第 3 条)。
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyA, "a", 10);
    index.ApplyPut(kKeyB, "b", 11);
    const auto removed = index.ApplySnapshot(kPrefix, {}, 50, /*complete=*/false);
    EXPECT_TRUE(removed.empty());
    EXPECT_EQ(index.size(), 2u);
}

TEST(DiscoveryKeyIndexTest, SnapshotPrunesTombstonesItAlreadyReflects)
{
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyA, "a", 10);
    index.ApplyDelete(kKeyA, 20);
    index.ApplyDelete(kKeyB, 70); // 本地没有记录,只记墓碑
    EXPECT_EQ(index.tombstoneCount(), 2u);
    // revision 50 的快照反映了 20 的删除,反映不了 70 的。不完整的快照照样清墓碑(它不删记录)。
    index.ApplySnapshot(kPrefix, {}, 50, /*complete=*/false);
    EXPECT_EQ(index.tombstoneCount(), 1u);
    EXPECT_FALSE(index.ApplyPut(kKeyB, "b", 60));
    EXPECT_TRUE(index.ApplyPut(kKeyA, "a", 15)) << "20 的墓碑已清:快照之后不再需要挡它";
}

TEST(DiscoveryKeyIndexTest, ReconcileResultsAreOrderedByKey)
{
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyB, "b", 11);
    index.ApplyPut(kKeyA, "a", 10);
    const auto removed = index.ApplySnapshot(kPrefix, {}, 50, true);
    EXPECT_EQ(Keys(removed), (std::vector<std::string>{kKeyA, kKeyB}));
    EXPECT_EQ(index.size(), 0u);
}

// ── 地址解析 ────────────────────────────────────────────────────────────

TEST(EtcdEndpointTest, AcceptsHostPortWithOptionalHttpSchemeAndTrailingSlash)
{
    etcd_transport::Endpoint endpoint;
    std::string error;
    ASSERT_TRUE(etcd_transport::ParseEndpoint("127.0.0.1:2379", endpoint, error)) << error;
    EXPECT_EQ(endpoint.host, "127.0.0.1");
    EXPECT_EQ(endpoint.port, 2379);

    ASSERT_TRUE(etcd_transport::ParseEndpoint("http://etcd.mmorpg-infra:2379/", endpoint, error)) << error;
    EXPECT_EQ(endpoint.host, "etcd.mmorpg-infra");
    EXPECT_EQ(endpoint.port, 2379);
}

TEST(EtcdEndpointTest, RejectsWhatTheTransportCannotDial)
{
    etcd_transport::Endpoint endpoint;
    std::string error;
    for (const char *bad : {"https://etcd:2379", "[::1]:2379", "etcd", ":2379", "etcd:", "etcd:0", "etcd:65536",
                            "etcd:23x9", "a:b:2379"})
    {
        error.clear();
        EXPECT_FALSE(etcd_transport::ParseEndpoint(bad, endpoint, error)) << bad;
        EXPECT_FALSE(error.empty()) << bad;
    }
}

// ── 流重建退避 ──────────────────────────────────────────────────────────

TEST(StreamRebuildBackoffTest, DoublesFromTwoHundredMillisecondsAndCapsAtFiveSeconds)
{
    etcd_transport::StreamRebuildBackoff backoff;
    std::vector<long long> delays;
    for (int i = 0; i < 8; ++i)
    {
        delays.push_back(backoff.OnStreamEnded(1s).count());
    }
    EXPECT_EQ(delays, (std::vector<long long>{200, 400, 800, 1600, 3200, 5000, 5000, 5000}))
        << "不设次数上限:封顶后一直按 5s 重建(§10.2 第 1 条)";
}

TEST(StreamRebuildBackoffTest, StreamThatLivedTenSecondsStartsOver)
{
    etcd_transport::StreamRebuildBackoff backoff;
    backoff.OnStreamEnded(0s);
    backoff.OnStreamEnded(0s);
    EXPECT_EQ(backoff.OnStreamEnded(0s), 800ms);
    EXPECT_EQ(backoff.OnStreamEnded(10s), 200ms) << "活过 10s 视为稳定过,从头开始";
    EXPECT_EQ(backoff.OnStreamEnded(9s), 400ms);
}

} // namespace
