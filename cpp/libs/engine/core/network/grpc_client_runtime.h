#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>

#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include "entt/src/entt/entity/registry.hpp"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

// 官方 gRPC 客户端的节点级寿命。只在节点所属 loop 线程访问；调用对象由对象池持有，
// 这里只借用其 context，完成标签被处理前不得析构调用或节点实体。
struct GrpcClientRuntime
{
    std::unordered_set<grpc::ClientContext *> calls;
    size_t queuedCompletions{0};
    size_t activeCompletions{0};
    bool closing{false};
    bool queueShutdown{false};
    bool drained{false};
};

using GrpcClientRuntimePtr = std::shared_ptr<GrpcClientRuntime>;

namespace grpc_client_runtime_detail
{
// 一个线程只拥有一个当前 EventLoop；计数覆盖尚无节点 runtime 的本地拒绝。
// 不放进 ECS：失效实体、缺组件等失败恰恰不能依赖一个有效 ECS owner。
inline thread_local size_t localCompletions{0};
}

[[nodiscard]] inline bool GrpcClientLocalCompletionsDrained()
{
    if (muduo::net::EventLoop::getEventLoopOfCurrentThread() == nullptr)
    {
        LOG_FATAL << "gRPC completion drain must be checked on its owning EventLoop thread";
    }
    return grpc_client_runtime_detail::localCompletions == 0;
}

// 覆盖整个标签派发栈，包括业务回调重入的内层轮询。
class GrpcClientCompletionScope
{
public:
    explicit GrpcClientCompletionScope(GrpcClientRuntimePtr runtime) : runtime_(std::move(runtime))
    {
        ++runtime_->activeCompletions;
    }
    ~GrpcClientCompletionScope() { --runtime_->activeCompletions; }
    GrpcClientCompletionScope(const GrpcClientCompletionScope &) = delete;
    GrpcClientCompletionScope &operator=(const GrpcClientCompletionScope &) = delete;

private:
    GrpcClientRuntimePtr runtime_;
};

inline bool HasUsableGrpcClient(const entt::registry &registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        return false;
    }
    const auto *runtime = registry.try_get<GrpcClientRuntimePtr>(entity);
    return runtime != nullptr && *runtime && !(*runtime)->closing &&
           registry.all_of<grpc::CompletionQueue>(entity);
}

// 缺组件与关闭期间的拒绝同真实 CQ 完成一样延后，避免发送栈内重入业务回调。
// 捕获调用自己的副本，不再借用 registry / 节点实体；节点排空也等待这些本地完成。
inline void QueueGrpcClientCompletion(GrpcClientRuntimePtr runtime, std::function<void()> complete)
{
    auto *loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();
    if (loop == nullptr)
    {
        LOG_FATAL << "gRPC client must be used on its owning EventLoop thread";
    }
    if (runtime)
    {
        ++runtime->queuedCompletions;
    }
    ++grpc_client_runtime_detail::localCompletions;
    try
    {
        loop->queueInLoop([runtime, complete = std::move(complete)]() mutable
        {
            struct CompletionFinished
            {
                GrpcClientRuntimePtr runtime;
                ~CompletionFinished()
                {
                    if (runtime) --runtime->queuedCompletions;
                    --grpc_client_runtime_detail::localCompletions;
                }
            } finished{runtime};
            complete();
        });
    }
    catch (...)
    {
        if (runtime) --runtime->queuedCompletions;
        --grpc_client_runtime_detail::localCompletions;
        throw;
    }
}
