#include <gtest/gtest.h>

#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// 官方 gRPC 只剩 PerCallDoneReceivesReplyOnceAndBypassesGlobalHandlers 里的服务端桩:客户端侧第 3 批已删除
// (docs/design/grpc-muduo-transport.md §14.1)。第 5 批删官方运行时时,这个桩要换成 muduo_grpc_test 的 TestH2Peer。
#include <grpcpp/grpcpp.h>
#include "proto/data_service/data_service.grpc.pb.h"
#include "core/network/grpc_client_runtime.h"
#include "grpc_client/data_service/data_service_grpc_client.h"
#include "muduo/net/EventLoop.h"
#include "muduo_grpc/channel.h"
#include "network/node_utils.h"
#include "thread_context/node_context_manager.h"
#include "thread_context/ecs_context.h"

namespace
{

// 真实生成客户端 + 节点实体上的 muduo_grpc::Channel(docs/design/grpc-muduo-transport.md §14);
// 不连接外部服务,不复制发送或排空算法。
class GrpcClientLifecycleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        oldLoad_ = std::move(data_service::AsyncDataServiceLoadPlayerDataHandler);
        oldLoadFailure_ = std::move(data_service::AsyncDataServiceLoadPlayerDataFailedHandler);
    }

    void TearDown() override
    {
        // 与 Node 摘节点 / 关机阶段 0 同序:先析构通道(~Channel 把在途调用以 UNAVAILABLE 排进队列),
        // 再转 loop 让排队的完成回调、连接释放、~TcpClient 排进的 forceClose / connectDestroyed 跑完。
        for (const auto &[type, entity] : entities_)
        {
            auto &registry = tlsNodeContextManager.GetRegistry(type);
            if (registry.valid(entity)) registry.destroy(entity);
        }
        // 建过通道的用例多转 1.2s:必须在 EventLoop 析构前跑完的是 Connector 排队的 resetChannel;没连上就析构的
        // TcpClient 另挂一个 1s 的 removeConnector 定时器,它是空函数,没到期时由 ~TimerQueue 删掉并释放 Connector
        // (那时 channel_ 已清空)也无害。1.2s 与 muduo_grpc_test 的 drainAfterTest_ 取同一个保守值,让它也走完。
        RunFor(channelCreated_ ? 1.2 : 0.05);
        EXPECT_TRUE(GrpcClientLocalCompletionsDrained()) << "本地拒绝的完成回调必须在 EventLoop 析构前全部执行";
        data_service::AsyncDataServiceLoadPlayerDataHandler = std::move(oldLoad_);
        data_service::AsyncDataServiceLoadPlayerDataFailedHandler = std::move(oldLoadFailure_);
    }

    // 每次调用回调测试共用:全局应答 / 失败处理器只计数,带 done 的调用不得触发它们。
    std::shared_ptr<int> CountGlobalLoadPlayerDataHandlers()
    {
        const auto calls = std::make_shared<int>(0);
        data_service::AsyncDataServiceLoadPlayerDataHandler =
            [calls](const GrpcReplyContext &, const data_service::LoadPlayerDataResponse &) { ++*calls; };
        data_service::AsyncDataServiceLoadPlayerDataFailedHandler =
            [calls](const GrpcCallFailure &, const data_service::LoadPlayerDataRequest &) { ++*calls; };
        return calls;
    }

    bool RunUntil(const std::function<bool()> &done, double timeoutSeconds = 5.0)
    {
        if (done()) return true;
        bool completed = false;
        const auto poll = loop_.runEvery(0.001, [&]
        {
            if (done())
            {
                completed = true;
                loop_.quit();
            }
        });
        const auto timeout = loop_.runAfter(timeoutSeconds, [&] { loop_.quit(); });
        loop_.loop();
        loop_.cancel(poll);
        loop_.cancel(timeout);
        return completed;
    }

    // 无条件转满 seconds:让排队的回调、连接释放跑完,也用来确认「恰好一次」之后没有第二次。
    void RunFor(double seconds)
    {
        (void)RunUntil([] { return false; }, seconds);
    }

    entt::entity EmptyNode(uint32_t type)
    {
        auto &registry = tlsNodeContextManager.GetRegistry(type);
        const auto entity = registry.create();
        entities_.emplace_back(type, entity);
        return entity;
    }

    // 与 NodeConnector::ConnectToGrpcNode 同形:节点实体上挂一条 muduo_grpc::Channel(MuduoGrpcChannelPtr)。
    // 默认端口 1 没人监听:只用来造「挂着通道、调用在途」的节点;用例在 loop 转起来之前就摘通道,不等建连失败。
    entt::entity InitNode(uint32_t type, uint16_t port = 1)
    {
        const auto entity = EmptyNode(type);
        auto &registry = tlsNodeContextManager.GetRegistry(type);
        auto channel = muduo_grpc::Channel::Create(loop_, "127.0.0.1", port);
        EXPECT_NE(channel, nullptr);
        registry.emplace<MuduoGrpcChannelPtr>(entity, std::move(channel));
        registry.emplace<NodeInfo>(entity).set_node_type(type);
        channelCreated_ = true;
        return entity;
    }

    muduo::net::EventLoop loop_;
    std::vector<std::pair<uint32_t, entt::entity>> entities_;
    bool channelCreated_{false};
    data_service::AsyncDataServiceLoadPlayerDataHandlerFunctionType oldLoad_;
    data_service::AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType oldLoadFailure_;
};

TEST_F(GrpcClientLifecycleTest, MissingTransportFailsAsynchronouslyOnceWithRequestAndMetadataCopies)
{
    auto &registry = tlsNodeContextManager.GetRegistry(common::base::DataServiceNodeService);
    const auto failures = std::make_shared<int>(0);
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [failures](const GrpcCallFailure &failure,
                                                                 const data_service::LoadPlayerDataRequest &request)
    {
        ++*failures;
        EXPECT_EQ(failure.code(), kGrpcStatusUnavailable);
        EXPECT_EQ(request.player_id(), 42u);
        const auto *metadata = failure.FindSentMetadata("x-test");
        ASSERT_NE(metadata, nullptr);
        EXPECT_EQ(*metadata, "snapshot");
    };
    // 0 = 失效代际;1 = 实体上没挂通道(只有 NodeInfo 的握手实体同属此类);2 = 通道组件为空指针。
    for (int missing = 0; missing != 3; ++missing)
    {
        const auto entity = EmptyNode(common::base::DataServiceNodeService);
        if (missing == 0) registry.destroy(entity); // 失效代际，不能用相同 index 猜继任实体。
        if (missing == 2) registry.emplace<MuduoGrpcChannelPtr>(entity);
        data_service::LoadPlayerDataRequest request;
        request.set_player_id(42);
        std::vector<std::string> values{"snapshot"};
        data_service::SendDataServiceLoadPlayerData(registry, entity, request, {"x-test"}, values);
        request.set_player_id(99);
        values.front() = "overwritten";
        EXPECT_EQ(*failures, missing) << "缺通道也不能在 Send 栈内同步回调";
        EXPECT_TRUE(RunUntil([&] { return *failures == missing + 1; }));
    }
    EXPECT_EQ(*failures, 3);
}

TEST_F(GrpcClientLifecycleTest, MissingChannelAndStaleSendsKeepAdjacentFinalizationGateClosed)
{
    const auto type = common::base::DataServiceNodeService;
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto missingChannel = EmptyNode(type);
    const auto stale = EmptyNode(type);
    registry.destroy(stale);
    const auto marker = tlsEcs.actorRegistry.create();
    struct CompletionState
    {
        bool adjacentFinalizerChecked{false};
        int failures{0};
    };
    const auto state = std::make_shared<CompletionState>();
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [state, marker]
        (const GrpcCallFailure &failure, const data_service::LoadPlayerDataRequest &)
    {
        EXPECT_EQ(failure.code(), kGrpcStatusUnavailable);
        EXPECT_TRUE(state->adjacentFinalizerChecked) << "本地失败必须排到下一 pending 批次";
        EXPECT_FALSE(GrpcClientLocalCompletionsDrained()) << "handler 退栈前，真正清理门槛仍须关闭";
        EXPECT_TRUE(tlsEcs.actorRegistry.valid(marker));
        ++state->failures;
    };
    EXPECT_TRUE(GrpcClientLocalCompletionsDrained());
    loop_.queueInLoop([&registry, missingChannel, stale]
    {
        data_service::LoadPlayerDataRequest request;
        data_service::SendDataServiceLoadPlayerData(registry, missingChannel, request);
        data_service::SendDataServiceLoadPlayerData(registry, stale, request);
    });
    // 与 Node 拆除段(FinalizeTeardownInLoop)共用同一个生产只读门槛;Send 排入下一批的无通道完成也必须可见。
    loop_.queueInLoop([state, marker]
    {
        state->adjacentFinalizerChecked = true;
        EXPECT_EQ(state->failures, 0);
        EXPECT_FALSE(GrpcClientLocalCompletionsDrained());
        EXPECT_TRUE(tlsEcs.actorRegistry.valid(marker));
    });
    EXPECT_TRUE(RunUntil([&] { return state->failures == 2 && GrpcClientLocalCompletionsDrained(); }));
    EXPECT_TRUE(tlsEcs.actorRegistry.valid(marker));
    if (tlsEcs.actorRegistry.valid(marker)) tlsEcs.actorRegistry.destroy(marker);
}

// 摘通道(Node 摘节点的 remove<MuduoGrpcChannelPtr> / 关机阶段 0 的 clear<MuduoGrpcChannelPtr> 是同一动作):
// 在途调用由 ~Channel 以 UNAVAILABLE 排队完成,之后的调用走本地拒绝;两者都恰好一次、都不在当前栈内回调,
// 失败处理器执行时通道已经析构(docs/design/grpc-muduo-transport.md §14.2)。
TEST_F(GrpcClientLifecycleTest, ChannelRemovalFailsOutstandingCallAndRejectsLaterCallThroughFailureHandler)
{
    const auto type = common::base::DataServiceNodeService;
    const auto entity = InitNode(type);
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const std::weak_ptr<muduo_grpc::Channel> weakChannel = registry.get<MuduoGrpcChannelPtr>(entity);
    const auto failedPlayers = std::make_shared<std::vector<uint64_t>>();
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [failedPlayers, weakChannel](const GrpcCallFailure &failure,
                                                                 const data_service::LoadPlayerDataRequest &failedRequest)
    {
        EXPECT_EQ(failure.code(), kGrpcStatusUnavailable);
        EXPECT_TRUE(weakChannel.expired()) << "失败回调在通道析构之后才执行,不得借用通道";
        failedPlayers->push_back(failedRequest.player_id());
    };
    data_service::LoadPlayerDataRequest request;
    request.set_player_id(43);
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    ASSERT_FALSE(weakChannel.expired());
    EXPECT_EQ(weakChannel.lock()->outstandingCalls(), 1u);
    registry.remove<MuduoGrpcChannelPtr>(entity);
    EXPECT_TRUE(weakChannel.expired()) << "生成层与调用状态都不得持有通道的强引用";
    EXPECT_FALSE(HasUsableGrpcClient(registry, entity));
    request.set_player_id(44);
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    EXPECT_TRUE(failedPlayers->empty()) << "摘通道与之后的本地拒绝都不得在当前栈内回调";
    EXPECT_FALSE(GrpcClientLocalCompletionsDrained()) << "摘通道后的调用走本地拒绝,不再发往网络";
    ASSERT_TRUE(RunUntil([&] { return failedPlayers->size() == 2 && GrpcClientLocalCompletionsDrained(); }));
    RunFor(0.05);
    ASSERT_EQ(failedPlayers->size(), 2u) << "每个调用恰好一次";
    EXPECT_EQ((*failedPlayers)[0], 43u) << "~Channel 先排队,在途调用先完成";
    EXPECT_EQ((*failedPlayers)[1], 44u);
}

TEST_F(GrpcClientLifecycleTest, SceneManagerSelectionRejectsSnapshotOnlyAndClosingEntities)
{
    const auto type = common::base::SceneManagerNodeService;
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto snapshotOnly = EmptyNode(type);
    registry.emplace<NodeInfo>(snapshotOnly).set_node_type(type);
    EXPECT_EQ(GetSceneManagerEntity(77), static_cast<entt::entity>(entt::null));
    const auto connected = InitNode(type);
    EXPECT_EQ(GetSceneManagerEntity(77), connected);
    // 摘节点先摘通道(Node 的服务节点下线处理:remove<MuduoGrpcChannelPtr> 之后才派发 OnNodeRemoveEvent)。
    registry.remove<MuduoGrpcChannelPtr>(connected);
    EXPECT_EQ(GetSceneManagerEntity(77), static_cast<entt::entity>(entt::null));
}

// 测试内的官方 C++ gRPC 服务端桩:对端是真实 HTTP/2 gRPC 服务端,muduo_grpc 客户端照常与之互通。
class LoadPlayerDataService final : public data_service::DataService::Service
{
public:
    std::atomic<int> calls{0};
    std::atomic<bool> sawMetadata{false};
    std::atomic<bool> sawEncodedValue{false};

private:
    grpc::Status LoadPlayerData(grpc::ServerContext *context, const data_service::LoadPlayerDataRequest *request,
                                data_service::LoadPlayerDataResponse *response) override
    {
        ++calls;
        const auto &metadata = context->client_metadata();
        const auto it = metadata.find("x-test");
        sawMetadata = it != metadata.end();
        // 应用层先 Base64 一层(docs/design/grpc-muduo-transport.md §4.2):原值 "snapshot" 在线上是 "c25hcHNob3Q="。
        sawEncodedValue = it != metadata.end() && std::string(it->second.data(), it->second.size()) == "c25hcHNob3Q=";
        response->set_version(request->player_id() + 1);
        return grpc::Status::OK;
    }
};

// 每次调用回调的结果记录。回调只在 loop 线程执行,不需要原子量。
struct PerCallDoneState
{
    int calls{0};
    uint32_t code{~0u};
    bool hadReply{false};
    uint64_t version{0};
};

GrpcDone<data_service::LoadPlayerDataResponse> RecordInto(const std::shared_ptr<PerCallDoneState> &state)
{
    return [state](const GrpcCallOutcome &outcome, const data_service::LoadPlayerDataResponse *reply)
    {
        ++state->calls;
        state->code = outcome.code;
        state->hadReply = reply != nullptr;
        if (reply != nullptr) state->version = reply->version();
    };
}

// 每次调用回调(grpc-client-deadline-failure-callback.md §11):成功时恰好一次、拿到 reply、不在 Send 栈内同步调用,
// options 的 deadline 与 metadata 生效,且不触发该方法的全局应答 / 失败处理器。
TEST_F(GrpcClientLifecycleTest, PerCallDoneReceivesReplyOnceAndBypassesGlobalHandlers)
{
    LoadPlayerDataService service;
    grpc::ServerBuilder builder;
    int port = 0;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    ASSERT_GT(port, 0);
    const auto type = common::base::DataServiceNodeService;
    const auto entity = InitNode(type, static_cast<uint16_t>(port));
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto globalCalls = CountGlobalLoadPlayerDataHandlers();
    const auto state = std::make_shared<PerCallDoneState>();

    data_service::LoadPlayerDataRequest request;
    request.set_player_id(41);
    GrpcCallOptions options;
    options.deadline = std::chrono::milliseconds(5000);
    options.metadata.emplace_back("x-test", "snapshot");
    data_service::SendDataServiceLoadPlayerData(registry, entity, request, RecordInto(state), options);
    request.set_player_id(99); // 调用对象持有请求副本,发出后改调用方的请求不影响本次调用
    EXPECT_EQ(state->calls, 0) << "done 不得在 Send 栈内同步调用";

    EXPECT_TRUE(RunUntil([&] { return state->calls == 1; }));
    EXPECT_EQ(state->code, 0u);
    EXPECT_TRUE(state->hadReply);
    EXPECT_EQ(state->version, 42u);
    EXPECT_EQ(service.calls.load(), 1);
    EXPECT_TRUE(service.sawMetadata.load()) << "options.metadata 要随请求发出";
    EXPECT_TRUE(service.sawEncodedValue.load()) << "metadata 原值发出前要做应用层 Base64(§4.2)";

    // 摘通道 = 节点摘除 / 关机阶段 0 的动作;已完成的调用不得因此再完成一次。
    registry.remove<MuduoGrpcChannelPtr>(entity);
    RunFor(0.2);
    EXPECT_EQ(state->calls, 1) << "摘通道后也不能有第二次完成";
    EXPECT_EQ(*globalCalls, 0) << "带 done 的调用不得再触发全局处理器";
    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));
}

// 实体没挂通道的本地拒绝:done 以 UNAVAILABLE 异步完成一次,reply 为空。
TEST_F(GrpcClientLifecycleTest, PerCallDoneLocalRejectIsAsynchronousOnceWithUnavailable)
{
    const auto type = common::base::DataServiceNodeService;
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto entity = EmptyNode(type);
    const auto globalCalls = CountGlobalLoadPlayerDataHandlers();
    const auto state = std::make_shared<PerCallDoneState>();

    data_service::LoadPlayerDataRequest request;
    data_service::SendDataServiceLoadPlayerData(registry, entity, request, RecordInto(state));
    EXPECT_EQ(state->calls, 0) << "本地拒绝也不得在 Send 栈内同步回调";
    EXPECT_FALSE(GrpcClientLocalCompletionsDrained()) << "排队中的本地完成要计入排空判据";

    EXPECT_TRUE(RunUntil([&] { return state->calls == 1 && GrpcClientLocalCompletionsDrained(); }));
    EXPECT_EQ(state->code, kGrpcStatusUnavailable);
    EXPECT_FALSE(state->hadReply);
    EXPECT_EQ(*globalCalls, 0);
}

// 摘通道:在途调用由 ~Channel 以 UNAVAILABLE 完成一次(执行时通道已析构),摘通道后的新调用以 UNAVAILABLE 本地拒绝一次;
// 两者都不在当前栈内回调、都不触发全局处理器。§14.2:停机 / 摘节点一律 UNAVAILABLE(调用方仍不得按 code 区分)。
TEST_F(GrpcClientLifecycleTest, PerCallDoneCompletesUnavailableAfterChannelIsRemoved)
{
    const auto type = common::base::DataServiceNodeService;
    const auto entity = InitNode(type);
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const std::weak_ptr<muduo_grpc::Channel> weakChannel = registry.get<MuduoGrpcChannelPtr>(entity);
    const auto globalCalls = CountGlobalLoadPlayerDataHandlers();
    const auto outstanding = std::make_shared<PerCallDoneState>();
    const auto rejected = std::make_shared<PerCallDoneState>();

    data_service::LoadPlayerDataRequest request;
    request.set_player_id(45);
    data_service::SendDataServiceLoadPlayerData(
        registry, entity, request,
        [outstanding, weakChannel](const GrpcCallOutcome &outcome, const data_service::LoadPlayerDataResponse *reply)
        {
            ++outstanding->calls;
            outstanding->code = outcome.code;
            outstanding->hadReply = reply != nullptr;
            EXPECT_TRUE(weakChannel.expired()) << "done 在通道析构之后才执行,不得捕获通道或实体组件";
        });
    ASSERT_FALSE(weakChannel.expired());
    EXPECT_EQ(weakChannel.lock()->outstandingCalls(), 1u);
    registry.remove<MuduoGrpcChannelPtr>(entity);
    EXPECT_TRUE(weakChannel.expired());
    data_service::SendDataServiceLoadPlayerData(registry, entity, request, RecordInto(rejected));
    EXPECT_EQ(outstanding->calls, 0) << "摘通道不得在当前栈内执行 done";
    EXPECT_EQ(rejected->calls, 0) << "本地拒绝也不得在 Send 栈内同步回调";
    EXPECT_FALSE(GrpcClientLocalCompletionsDrained()) << "摘通道后的调用走本地拒绝,不再发往网络";

    ASSERT_TRUE(RunUntil([&]
    {
        return outstanding->calls == 1 && rejected->calls == 1 && GrpcClientLocalCompletionsDrained();
    }));
    RunFor(0.05);
    EXPECT_EQ(outstanding->calls, 1) << "恰好一次";
    EXPECT_EQ(outstanding->code, kGrpcStatusUnavailable);
    EXPECT_FALSE(outstanding->hadReply);
    EXPECT_EQ(rejected->calls, 1);
    EXPECT_EQ(rejected->code, kGrpcStatusUnavailable);
    EXPECT_FALSE(rejected->hadReply);
    EXPECT_EQ(*globalCalls, 0);
}

} // namespace
