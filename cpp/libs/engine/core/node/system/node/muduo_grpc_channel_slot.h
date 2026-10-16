#pragma once

// gRPC 节点实体上的通道组件(docs/design/grpc-muduo-transport.md §14)。
//
// 2026-10-15 第 3 批起所有出站 gRPC 都走 muduo 驱动的传输:NodeConnector::ConnectToGrpcNode 给每个 gRPC 节点
// 建一条 muduo_grpc::Channel,挂成 MuduoGrpcChannelPtr 组件;生成的 Send<Svc><Method> 从这里取通道。
// 组件随节点实体销毁(节点摘除),关机时由 Node::FinalizeShutdownInLoop 阶段 0 统一清掉(~Channel 把在途调用以
// UNAVAILABLE 排进队列,拆除排在它们之后)。通道必须在其 EventLoop 线程上、loop 仍在运行时析构。
//
// 这里只前向声明通道类型:std::shared_ptr<T> 的析构不需要 T 的完整定义(删除器在创建处记进控制块),
// 只持有 / 清理组件的翻译单元不必包含 muduo_grpc 的头。

#include <memory>

namespace muduo_grpc
{
class Channel;
} // namespace muduo_grpc

using MuduoGrpcChannelPtr = std::shared_ptr<muduo_grpc::Channel>;
