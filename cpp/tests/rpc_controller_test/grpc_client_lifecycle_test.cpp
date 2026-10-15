#include <gtest/gtest.h>

#include <chrono>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "core/network/grpc_client_runtime.h"
#include "grpc_client/data_service/data_service_grpc_client.h"
#include "grpc_client/etcd/etcd_grpc_client.h"
#include "grpc_client/grpc_init_client.h"
#include "muduo/net/EventLoop.h"
#include "network/node_utils.h"
#include "thread_context/node_context_manager.h"
#include "thread_context/ecs_context.h"

namespace
{

// 真实生成客户端和官方 CQ；不连接外部服务，不复制发送或排空算法。
class GrpcClientLifecycleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        oldLoadFailure_ = std::move(data_service::AsyncDataServiceLoadPlayerDataFailedHandler);
        oldWatch_ = std::move(etcdserverpb::AsyncWatchWatchHandler);
    }

    void TearDown() override
    {
        for (const auto &[type, entity] : entities_)
        {
            BeginShutdownGrpcNode(tlsNodeContextManager.GetRegistry(type), entity);
        }
        const bool drained = RunUntil([this]
        {
            bool all = true;
            for (const auto &[type, entity] : entities_)
            {
                if (!PollGrpcNodeShutdown(tlsNodeContextManager.GetRegistry(type), entity)) all = false;
            }
            return all && GrpcClientLocalCompletionsDrained();
        });
        EXPECT_TRUE(drained) << "取消后仍有 gRPC 标签未回收，不得销毁 CQ";
        if (drained)
        {
            for (const auto &[type, entity] : entities_)
            {
                auto &registry = tlsNodeContextManager.GetRegistry(type);
                if (registry.valid(entity)) registry.destroy(entity);
            }
        }
        data_service::AsyncDataServiceLoadPlayerDataFailedHandler = std::move(oldLoadFailure_);
        etcdserverpb::AsyncWatchWatchHandler = std::move(oldWatch_);
    }

    bool RunUntil(const std::function<bool()> &done)
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
        const auto timeout = loop_.runAfter(5.0, [&] { loop_.quit(); });
        loop_.loop();
        loop_.cancel(poll);
        loop_.cancel(timeout);
        return completed;
    }

    entt::entity EmptyNode(uint32_t type)
    {
        auto &registry = tlsNodeContextManager.GetRegistry(type);
        const auto entity = registry.create();
        entities_.emplace_back(type, entity);
        return entity;
    }

    entt::entity InitNode(uint32_t type, const std::string &target = "127.0.0.1:1")
    {
        const auto entity = EmptyNode(type);
        auto &registry = tlsNodeContextManager.GetRegistry(type);
        auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
        registry.emplace<std::shared_ptr<grpc::Channel>>(entity, channel);
        InitGrpcNode(channel, registry, entity);
        registry.emplace<NodeInfo>(entity).set_node_type(type);
        return entity;
    }

    muduo::net::EventLoop loop_;
    std::vector<std::pair<uint32_t, entt::entity>> entities_;
    data_service::AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType oldLoadFailure_;
    etcdserverpb::AsyncWatchWatchHandlerFunctionType oldWatch_;
};

TEST_F(GrpcClientLifecycleTest, MissingTransportFailsAsynchronouslyOnceWithRequestAndMetadataCopies)
{
    auto &registry = tlsNodeContextManager.GetRegistry(common::base::DataServiceNodeService);
    const auto failures = std::make_shared<int>(0);
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [failures](const GrpcCallFailure &failure,
                                                                 const data_service::LoadPlayerDataRequest &request)
    {
        ++*failures;
        EXPECT_EQ(failure.status.error_code(), grpc::StatusCode::UNAVAILABLE);
        EXPECT_EQ(request.player_id(), 42u);
        const auto *metadata = failure.FindSentMetadata("x-test");
        ASSERT_NE(metadata, nullptr);
        EXPECT_EQ(*metadata, "snapshot");
    };
    for (int missing = 0; missing != 5; ++missing)
    {
        const auto entity = EmptyNode(common::base::DataServiceNodeService);
        if (missing == 0) registry.destroy(entity); // 失效代际，不能用相同 index 猜继任实体。
        if (missing >= 2)
        {
            registry.emplace<GrpcClientRuntimePtr>(entity, std::make_shared<GrpcClientRuntime>());
            if (missing != 2) registry.emplace<grpc::CompletionQueue>(entity);
            if (missing != 3)
            {
                auto channel = grpc::CreateChannel("127.0.0.1:1", grpc::InsecureChannelCredentials());
                auto &stub = registry.emplace<data_service::DataServiceStubPtr>(entity, data_service::DataService::NewStub(channel));
                if (missing == 4) stub.reset();
            }
        }
        data_service::LoadPlayerDataRequest request;
        request.set_player_id(42);
        std::vector<std::string> values{"snapshot"};
        data_service::SendDataServiceLoadPlayerData(registry, entity, request, {"x-test"}, values);
        request.set_player_id(99);
        values.front() = "overwritten";
        EXPECT_EQ(*failures, missing) << "缺组件也不能在 Send 栈内同步回调";
        EXPECT_TRUE(RunUntil([&] { return *failures == missing + 1; }));
    }
    EXPECT_EQ(*failures, 5);
}

TEST_F(GrpcClientLifecycleTest, MissingRuntimeAndStaleSendsKeepAdjacentFinalizationGateClosed)
{
    const auto type = common::base::DataServiceNodeService;
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto missingRuntime = EmptyNode(type);
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
        EXPECT_EQ(failure.status.error_code(), grpc::StatusCode::UNAVAILABLE);
        EXPECT_TRUE(state->adjacentFinalizerChecked) << "本地失败必须排到下一 pending 批次";
        EXPECT_FALSE(GrpcClientLocalCompletionsDrained()) << "handler 退栈前，真正清理门槛仍须关闭";
        EXPECT_TRUE(tlsEcs.actorRegistry.valid(marker));
        ++state->failures;
    };
    EXPECT_TRUE(GrpcClientLocalCompletionsDrained());
    loop_.queueInLoop([&registry, missingRuntime, stale]
    {
        data_service::LoadPlayerDataRequest request;
        data_service::SendDataServiceLoadPlayerData(registry, missingRuntime, request);
        data_service::SendDataServiceLoadPlayerData(registry, stale, request);
    });
    // 与 Node 最终清池共用同一个生产只读门槛；Send 排入下一批的无 runtime 完成也必须可见。
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

TEST_F(GrpcClientLifecycleTest, ShutdownCancelsOutstandingCallAndRejectsNewCallBeforeQueueDestruction)
{
    const auto type = common::base::DataServiceNodeService;
    const auto entity = InitNode(type);
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto runtime = registry.get<GrpcClientRuntimePtr>(entity);
    const auto failures = std::make_shared<int>(0);
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [failures, runtime, &registry, entity](const GrpcCallFailure &failure,
                                                                 const data_service::LoadPlayerDataRequest &failedRequest)
    {
        EXPECT_FALSE(failure.status.ok());
        if (failedRequest.player_id() == 43)
        {
            EXPECT_EQ(runtime->calls.count(const_cast<grpc::ClientContext *>(&failure.context)), 1u)
                << "真实 unary 的 context 必须保留到业务 handler 返回";
            EXPECT_GE(runtime->activeCompletions, 1u);
        }
        else
        {
            EXPECT_GE(runtime->queuedCompletions, 1u);
        }
        EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity)) << "业务回调执行期间不能宣告排空";
        EXPECT_TRUE(registry.valid(entity));
        ++*failures;
    };
    data_service::LoadPlayerDataRequest request;
    request.set_player_id(43);
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    EXPECT_EQ(runtime->calls.size(), 1u);
    BeginShutdownGrpcNode(registry, entity);
    EXPECT_FALSE(HasUsableGrpcClient(registry, entity));
    request.set_player_id(44);
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    EXPECT_EQ(runtime->calls.size(), 1u) << "关闭后的请求不得向 CQ 追加网络操作";
    ASSERT_TRUE(RunUntil([&] { return PollGrpcNodeShutdown(registry, entity); }));
    EXPECT_EQ(*failures, 2);
    EXPECT_TRUE(runtime->calls.empty());
    EXPECT_EQ(runtime->queuedCompletions, 0u);
    EXPECT_TRUE(runtime->drained);
    EXPECT_TRUE(PollGrpcNodeShutdown(registry, entity)) << "重复轮询不能再次回调";
    EXPECT_EQ(*failures, 2);
}

TEST_F(GrpcClientLifecycleTest, QueuedFailureKeepsOwnerAliveDuringReentryAndAfterCqHasDrained)
{
    const auto type = common::base::DataServiceNodeService;
    const auto entity = InitNode(type);
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto runtime = registry.get<GrpcClientRuntimePtr>(entity);
    registry.get<data_service::DataServiceStubPtr>(entity).reset();
    const auto failures = std::make_shared<int>(0);
    data_service::AsyncDataServiceLoadPlayerDataFailedHandler = [failures, runtime, &registry, entity]
        (const GrpcCallFailure &, const data_service::LoadPlayerDataRequest &)
    {
        EXPECT_EQ(runtime->queuedCompletions, 1u);
        BeginShutdownGrpcNode(registry, entity);
        EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity))
            << "本地失败回调尚未返回，CQ 即使为空也不能释放 owner";
        EXPECT_TRUE(registry.valid(entity));
        ++*failures;
    };
    data_service::LoadPlayerDataRequest request;
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    EXPECT_TRUE(RunUntil([&] { return *failures == 1 && PollGrpcNodeShutdown(registry, entity); }));
    ASSERT_TRUE(runtime->drained);
    // CQ 已经 SHUTDOWN 后的拒绝仍是异步完成；drained 不能绕过后追加的业务回调。
    data_service::SendDataServiceLoadPlayerData(registry, entity, request);
    EXPECT_EQ(runtime->queuedCompletions, 1u);
    EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity));
    EXPECT_TRUE(registry.valid(entity));
    EXPECT_TRUE(RunUntil([&] { return *failures == 2 && PollGrpcNodeShutdown(registry, entity); }));
    EXPECT_EQ(runtime->queuedCompletions, 0u);
    EXPECT_EQ(runtime->activeCompletions, 0u);
}

TEST_F(GrpcClientLifecycleTest, SceneManagerSelectionRejectsSnapshotOnlyAndClosingEntities)
{
    const auto type = common::base::SceneManagerNodeService;
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    const auto snapshotOnly = EmptyNode(type);
    registry.emplace<NodeInfo>(snapshotOnly).set_node_type(type);
    EXPECT_EQ(GetSceneManagerEntity(77), entt::null);
    const auto connected = InitNode(type);
    EXPECT_EQ(GetSceneManagerEntity(77), connected);
    BeginShutdownGrpcNode(registry, connected);
    EXPECT_EQ(GetSceneManagerEntity(77), entt::null);
}

class WatchService final : public etcdserverpb::Watch::Service
{
public:
    std::atomic<int> repliesSent{0};

private:
    grpc::Status Watch(grpc::ServerContext *,
                       grpc::ServerReaderWriter<etcdserverpb::WatchResponse, etcdserverpb::WatchRequest> *stream) override
    {
        etcdserverpb::WatchRequest request;
        int64_t watchId = 77;
        while (stream->Read(&request))
        {
            etcdserverpb::WatchResponse response;
            response.set_watch_id(watchId++);
            if (!stream->Write(response)) break;
            ++repliesSent;
        }
        return grpc::Status::OK;
    }
};

TEST_F(GrpcClientLifecycleTest, StreamReadsIntoStableCallBufferAndCancellationDrainsEveryTag)
{
    WatchService service;
    grpc::ServerBuilder builder;
    int port = 0;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    ASSERT_GT(port, 0);
    const auto type = common::base::EtcdNodeService;
    const auto entity = InitNode(type, "127.0.0.1:" + std::to_string(port));
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    auto &client = registry.get<etcdserverpb::AsyncWatchWatchGrpcClient>(entity);
    const auto runtime = registry.get<GrpcClientRuntimePtr>(entity);
    const auto *buffer = &client.reply;
    const auto replies = std::make_shared<int>(0);
    etcdserverpb::AsyncWatchWatchHandler = [replies, buffer, runtime, &client, &registry, entity](const grpc::ClientContext &, const etcdserverpb::WatchResponse &reply)
    {
        ++*replies;
        EXPECT_EQ(&reply, buffer) << "异步 Read 必须借用稳定调用对象的缓冲";
        EXPECT_EQ(reply.watch_id(), 77);
        // 外层 READ 业务栈中还有真实 WRITE completion；内层处理完写入也不能先 Finish。
        etcdserverpb::WatchRequest additional;
        additional.mutable_create_request()->set_key("another/");
        etcdserverpb::SendWatchWatch(registry, entity, additional);
        EXPECT_TRUE(client.writing);
        BeginShutdownGrpcNode(registry, entity);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (client.writing && std::chrono::steady_clock::now() < deadline)
        {
            EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity));
        }
        EXPECT_FALSE(client.writing) << "真实 WRITE completion 已在 READ handler 的重入轮询里完成";
        EXPECT_TRUE(client.reading) << "reply 借用未结束，不能重新 Read 或先 Finish";
        EXPECT_FALSE(client.finished);
        EXPECT_FALSE(runtime->calls.empty());
        EXPECT_GE(runtime->activeCompletions, 1u) << "外层 READ 派发尚未退栈";
        EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity));
        EXPECT_TRUE(registry.valid(entity));
        EXPECT_EQ(reply.watch_id(), 77);
    };
    // 删除同类型响应组件会发生 swap-and-pop；不得挪动正在异步读取的缓冲。
    const auto other = registry.create();
    registry.emplace<etcdserverpb::WatchResponse>(other);
    registry.destroy(other);
    etcdserverpb::WatchRequest request;
    request.mutable_create_request()->set_key("test/");
    etcdserverpb::SendWatchWatch(registry, entity, request);
    const bool completed = RunUntil([&]
    {
        const bool drained = PollGrpcNodeShutdown(registry, entity);
        return *replies == 1 && drained;
    });
    EXPECT_TRUE(completed);
    BeginShutdownGrpcNode(registry, entity);
    EXPECT_TRUE(RunUntil([&] { return PollGrpcNodeShutdown(registry, entity); }));
    EXPECT_TRUE(registry.get<GrpcClientRuntimePtr>(entity)->calls.empty());
    EXPECT_EQ(*replies, 1);
    // LeaseKeepAlive 未注册，INIT/READ 的 ok=false 也必须被处理并回收，而不能吞掉 Watch 的后续标签。
    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));
}

TEST_F(GrpcClientLifecycleTest, ReentrantWriteCompletionCannotReuseReplyBorrowedByReadHandler)
{
    WatchService service;
    grpc::ServerBuilder builder;
    int port = 0;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    ASSERT_GT(port, 0);
    const auto type = common::base::EtcdNodeService;
    const auto entity = InitNode(type, "127.0.0.1:" + std::to_string(port));
    auto &registry = tlsNodeContextManager.GetRegistry(type);
    auto &client = registry.get<etcdserverpb::AsyncWatchWatchGrpcClient>(entity);
    struct DeliveryState
    {
        int replies{0};
        bool firstHandlerActive{false};
    };
    const auto state = std::make_shared<DeliveryState>();
    etcdserverpb::AsyncWatchWatchHandler = [state, &service, &registry, &client, entity]
        (const grpc::ClientContext &, const etcdserverpb::WatchResponse &reply)
    {
        ++state->replies;
        if (state->replies == 1)
        {
            state->firstHandlerActive = true;
            EXPECT_TRUE(client.reading) << "读取结束后的缓冲借用期也必须阻止新 Read";
            EXPECT_EQ(reply.watch_id(), 77);
            const auto *borrowed = &reply;
            etcdserverpb::WatchRequest additional;
            additional.mutable_create_request()->set_key("second/");
            etcdserverpb::SendWatchWatch(registry, entity, additional);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while ((client.writing || service.repliesSent.load() < 2) && std::chrono::steady_clock::now() < deadline)
            {
                EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity));
            }
            EXPECT_FALSE(client.writing);
            EXPECT_GE(service.repliesSent.load(), 2) << "服务端已经写出第二份真实响应";
            EXPECT_FALSE(PollGrpcNodeShutdown(registry, entity));
            EXPECT_EQ(state->replies, 1) << "第一份 reply 仍借出，第二个 READ handler 不得重入";
            EXPECT_EQ(&reply, borrowed);
            EXPECT_EQ(reply.watch_id(), 77) << "内层 WRITE completion 不得重新 Read 并覆写外层 reply";
            state->firstHandlerActive = false;
            return;
        }
        EXPECT_FALSE(state->firstHandlerActive);
        EXPECT_EQ(state->replies, 2);
        EXPECT_EQ(reply.watch_id(), 78);
        BeginShutdownGrpcNode(registry, entity);
    };
    etcdserverpb::WatchRequest request;
    request.mutable_create_request()->set_key("first/");
    etcdserverpb::SendWatchWatch(registry, entity, request);
    EXPECT_TRUE(RunUntil([&] { return PollGrpcNodeShutdown(registry, entity) && state->replies == 2; }));
    BeginShutdownGrpcNode(registry, entity);
    EXPECT_TRUE(RunUntil([&] { return PollGrpcNodeShutdown(registry, entity); }));
    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(1));
}

} // namespace
