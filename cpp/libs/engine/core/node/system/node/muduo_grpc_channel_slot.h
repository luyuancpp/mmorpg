#pragma once

// 迁移期接缝:让某个 gRPC 节点类型改走 muduo 驱动的传输(docs/design/grpc-muduo-transport.md §8)。
//
// core 不链接 muduo_grpc:这里只前向声明通道类型。std::shared_ptr<T> 的析构不需要 T 的完整定义
// (删除器在创建处就记进控制块),所以 core 可以持有、清理这个组件,而不引入任何链接依赖。
//
// 用法:进程在服务发现开始前,往目标节点类型的 registry 上下文里放一个 MuduoGrpcChannelFactory
// (tlsNodeContextManager.GetRegistry(type).ctx().emplace<MuduoGrpcChannelFactory>(...))。
// NodeConnector::ConnectToGrpcNode 看到工厂就用它建通道并挂成 MuduoGrpcChannelPtr 组件,
// **不再创建**官方 grpc::Channel、Stub 与 CompletionQueue;没有工厂的类型行为不变。
// 组件随节点实体销毁(节点摘除),关机时由 Node::FinalizeShutdownInLoop 在两跳排空之前统一清掉。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace muduo_grpc
{
class Channel;
} // namespace muduo_grpc

using MuduoGrpcChannelPtr = std::shared_ptr<muduo_grpc::Channel>;

struct MuduoGrpcChannelFactory
{
    // 返回空指针 = 无法建通道(例如地址非法);调用方记 ERROR 并放弃这个节点。
    std::function<MuduoGrpcChannelPtr(const std::string &ip, uint32_t port)> create;
};
