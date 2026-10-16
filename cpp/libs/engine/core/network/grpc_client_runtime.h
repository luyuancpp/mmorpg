#pragma once

// 生成的 gRPC 客户端在节点实体上的运行期接缝(docs/design/grpc-muduo-transport.md §14)。
//
// 第 3 批起传输是 muduo_grpc::Channel(实体上的 MuduoGrpcChannelPtr 组件);官方 CompletionQueue、
// GrpcClientRuntime 计数与 5ms 轮询都已删除。只在节点所属 EventLoop 线程使用。

#include <functional>
#include <memory>
#include <utility>

#include "entt/src/entt/entity/registry.hpp"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo_grpc/channel.h"
// 用 engine 根目录相对路径:生成的客户端(grpc_client 工程)只把 cpp/libs/engine 放进包含路径。
#include "core/node/system/node/muduo_grpc_channel_slot.h"

// 实体上的通道;实体无效或没挂通道返回空。
inline MuduoGrpcChannelPtr FindGrpcChannel(const entt::registry &registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        return nullptr;
    }
    const auto *slot = registry.try_get<MuduoGrpcChannelPtr>(entity);
    return slot != nullptr ? *slot : nullptr;
}

// 这个节点实体现在能不能发 gRPC:挂着通道且通道没关。不代表已经连上(选点按 ConnectivityState 分级另看)。
inline bool HasUsableGrpcClient(const entt::registry &registry, entt::entity entity)
{
    const auto channel = FindGrpcChannel(registry, entity);
    return channel && channel->GetState(false) != muduo_grpc::ConnectivityState::kShutdown;
}

// 本地就失败的调用(实体没有通道、请求序列化失败)与真实应答一样延后到下一轮投递:
// 保持「完成回调永不在 Send 栈内同步执行」。complete 捕获调用自己的状态副本,不借用 registry / 节点实体。
inline void QueueGrpcLocalCompletion(std::function<void()> complete)
{
    auto *loop = muduo::net::EventLoop::getEventLoopOfCurrentThread();
    if (loop == nullptr)
    {
        LOG_FATAL << "gRPC client must be used on its owning EventLoop thread";
    }
    loop->queueInLoop(std::move(complete));
}
