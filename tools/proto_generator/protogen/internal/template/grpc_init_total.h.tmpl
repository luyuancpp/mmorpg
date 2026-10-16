#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <google/protobuf/message.h>
#include "grpc_client/grpc_call_tag.h"

// 生成的 gRPC 客户端的进程级入口(docs/design/grpc-muduo-transport.md §14)。传输是节点实体上的
// muduo_grpc::Channel(NodeConnector::ConnectToGrpcNode 建),这里不再有 CompletionQueue 初始化、轮询与排空。

void SetIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);

void SetHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);

// 失败处理器(unary 调用以非 OK 状态结束时),与 SetIfEmptyHandler / SetHandler 平行。流式方法不生成客户端。
void SetIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);

void SetFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);

// 发往 nodeType 类节点的所有 unary 调用此后使用的 deadline。
// 由 grpc_call_deadline::Apply 在 Node::Initialize 里按 BaseDeployConfig.grpc_client 调用。
void SetGrpcCallDeadline(uint32_t nodeType, std::chrono::milliseconds deadline);
