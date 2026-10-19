#pragma once

#include <chrono>
#include <cstdint>

// 生成客户端的进程级 deadline 入口。每次调用必须显式提供完成回调。
// 由 grpc_call_deadline::Apply 在 Node::Initialize 中按部署配置调用。
void SetGrpcCallDeadline(uint32_t nodeType, std::chrono::milliseconds deadline);
