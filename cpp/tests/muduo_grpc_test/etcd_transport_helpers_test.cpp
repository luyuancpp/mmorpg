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
    EXPECT_TRUE(index.ApplyPut(kKeyA, "{\"v\":1}", 10).accepted);
    EXPECT_TRUE(index.ApplyPut(kKeyA, "{\"v\":2}", 12).accepted);

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
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v", 10).accepted);
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v", 10).accepted);
    EXPECT_EQ(index.size(), 1u);
}

TEST(DiscoveryKeyIndexTest, OlderPutIsStale)
{
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "new", 20).accepted);
    EXPECT_FALSE(index.ApplyPut(kKeyA, "old", 19).accepted) << "Range 快照比已处理的 watch PUT 旧";
    ASSERT_NE(index.FindValue(kKeyA), nullptr);
    EXPECT_EQ(*index.FindValue(kKeyA), "new");
}

TEST(DiscoveryKeyIndexTest, PutOlderThanAProcessedDeleteIsStaleButARecreateIsNot)
{
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v1", 10).accepted);
    EXPECT_EQ(index.ApplyDelete(kKeyA, 20).result, DeleteResult::kRemoved);
    // 删除之前拍下的快照晚到:不得把已下线的节点加回来。
    EXPECT_FALSE(index.ApplyPut(kKeyA, "v1", 10).accepted);
    EXPECT_EQ(index.FindValue(kKeyA), nullptr);
    // 删除之后重新注册(更大的 mod_revision):照常接受,墓碑随之清掉。
    EXPECT_TRUE(index.ApplyPut(kKeyA, "v2", 25).accepted);
    EXPECT_EQ(index.tombstoneCount(), 0u);
}

TEST(DiscoveryKeyIndexTest, DeleteOlderThanTheKnownPutIsStaleAndChangesNothing)
{
    // 快照(R ≥ 30)先于 watch 的旧 DELETE(20)到达:key 在 20 被删、30 又建了,旧删除必须忽略。
    DiscoveryKeyIndex index;
    EXPECT_TRUE(index.ApplyPut(kKeyA, "recreated", 30).accepted);
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
    EXPECT_FALSE(index.ApplyPut(kKeyB, "b", 11).accepted);
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
    EXPECT_FALSE(index.ApplyPut(kKeyB, "b", 60).accepted);
    // 20 的墓碑已清,索引本身不再挡比它旧的输入:比 revision 50 更旧的 Range 快照由 EtcdService 的
    // 每前缀已应用快照水位整条丢弃,watch 也只从水位之后建,所以这样的 PUT 到不了索引。
    EXPECT_TRUE(index.ApplyPut(kKeyA, "a", 15).accepted);
}

TEST(DiscoveryKeyIndexTest, PutThatReplacesAnotherHoldersValueReportsTheOldValue)
{
    // 流断期间 U1 下线、U2 拿回同一 node_id 写回同一个键:快照里键还在,只是值换了。
    // 调用方要拿到旧值去摘 U1,否则 U1 永久留成幽灵(发现快照按 uuid 区分节点)。
    DiscoveryKeyIndex index;
    const auto first = index.ApplyPut(kKeyA, "{\"node_uuid\":\"u1\"}", 10);
    EXPECT_TRUE(first.accepted);
    EXPECT_FALSE(first.replacedValue.has_value()) << "新键没有旧值";

    const auto replaced = index.ApplyPut(kKeyA, "{\"node_uuid\":\"u2\"}", 40);
    EXPECT_TRUE(replaced.accepted);
    ASSERT_TRUE(replaced.replacedValue.has_value());
    EXPECT_EQ(*replaced.replacedValue, "{\"node_uuid\":\"u1\"}");

    const auto same = index.ApplyPut(kKeyA, "{\"node_uuid\":\"u2\"}", 40);
    EXPECT_TRUE(same.accepted);
    EXPECT_FALSE(same.replacedValue.has_value()) << "同一版本重复到达不算换值";

    // U1 的 DELETE(revision 20)晚到:比已知 PUT 旧,判过时 —— 摘 U1 只能靠上面的旧值。
    EXPECT_EQ(index.ApplyDelete(kKeyA, 20).result, DeleteResult::kStale);
}

TEST(DiscoveryKeyIndexTest, HolderStaysRegisteredWhileAnyOfItsKeysRemains)
{
    // Go 侧 ReallocateNewID:进程失租后原 node_id 被别人占走,本进程保留 uuid u、换到新 node_id 继续服务,
    // 于是 u 从 K3 搬到 K10,K3 换成别人的 B。发现层按 uuid 摘节点,只能在 u 一个键都不剩时才摘。
    constexpr char kKey3[] = "FriendNodeService.rpc/zone/1/node_type/5/node_id/3";
    constexpr char kKey10[] = "FriendNodeService.rpc/zone/1/node_type/5/node_id/10";
    DiscoveryKeyIndex index;
    index.ApplyPut(kKey3, "{u@3}", 10, "u");
    EXPECT_TRUE(index.HoldsAnyKey("u"));

    // 同一个快照里(按键序 node_id/10 在 node_id/3 之前):K10=u,K3=B。
    index.ApplyPut(kKey10, "{u@10}", 30, "u");
    const auto replaced = index.ApplyPut(kKey3, "{B@3}", 31, "b");
    ASSERT_TRUE(replaced.replacedValue.has_value());
    EXPECT_EQ(replaced.replacedHolder, "u");
    EXPECT_TRUE(index.HoldsAnyKey("u")) << "u 还挂在 K10 上:调用方不得摘它";
    EXPECT_TRUE(index.HoldsAnyKey("b"));

    // 同一 uuid 挂两个键时其中一个被删(match 自己的注册表把旧键误判成被占而换号):u 仍在。
    const auto deleted = index.ApplyDelete(kKey10, 40);
    EXPECT_EQ(deleted.result, DeleteResult::kRemoved);
    EXPECT_EQ(deleted.holder, "u");
    EXPECT_FALSE(index.HoldsAnyKey("u")) << "最后一个键也没了才算下线";

    // 对账删除同样维护计数;解析不了的值(空持有者)不计入。
    index.ApplyPut(kKey10, "{garbage", 50, "");
    EXPECT_FALSE(index.HoldsAnyKey(""));
    const auto removed = index.ApplySnapshot("FriendNodeService.rpc", {}, 60, /*complete=*/true);
    EXPECT_EQ(removed.size(), 2u);
    EXPECT_FALSE(index.HoldsAnyKey("b"));
}

TEST(DiscoveryKeyIndexTest, ReplayedDeleteIsStale)
{
    // 重建后的 watch 从较早的 revision 回放,同一个删除再到一次:不能再交给发现层(会重置宽限期)。
    DiscoveryKeyIndex index;
    index.ApplyPut(kKeyA, "a", 10);
    EXPECT_EQ(index.ApplyDelete(kKeyA, 20).result, DeleteResult::kRemoved);
    EXPECT_EQ(index.ApplyDelete(kKeyA, 20).result, DeleteResult::kStale);
    EXPECT_EQ(index.ApplyDelete(kKeyA, 15).result, DeleteResult::kStale);
    // 对账删除同样记了墓碑:之后才到的那次真实 DELETE 也是重放。
    index.ApplyPut(kKeyB, "b", 11);
    index.ApplySnapshot(kPrefix, {}, 50, /*complete=*/true);
    EXPECT_EQ(index.ApplyDelete(kKeyB, 30).result, DeleteResult::kStale);
    // 更晚的删除(墓碑之后又有人建了键、本地没收到 PUT)照常处理。
    EXPECT_EQ(index.ApplyDelete(kKeyB, 60).result, DeleteResult::kUnknownKey);
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
