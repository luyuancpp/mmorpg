#pragma once

// 官方 gRPC 运行时探针(gRPC 迁到 muduo 驱动第 5 批;用法见 docs/design/thread-count-monitoring_zh.md「gRPC 运行时守卫」)。
//
// 第 3、4 批之后,节点内的 gRPC 客户端与服务端都走 muduo_grpc(跑在节点自己的 EventLoop 上,不建线程),
// 进程里不应再有任何代码初始化官方 gRPC 运行时。官方运行时一旦被初始化(显式 grpc_init(),或构造
// 继承 grpc::internal::GrpcLibrary 的官方对象时隐式调用:grpc::Channel(grpc::CreateChannel)、grpc::Server
// (ServerBuilder::BuildAndStart)、grpc::CompletionQueue、通道 / 服务端凭据、grpc::ResourceQuota、grpc::Alarm),
// 就会起 EventEngine 线程池、定时器等节点控制不了的线程,线程数不再可控。
// 本探针只回答「现在是否处于已初始化状态」(grpc_init 计数是否大于 0),供线程观测的守卫(thread_observability.h)
// 与回归用例(cpp/tests/rpc_controller_test/grpc_runtime_not_initialized_test.cpp)使用;grpc 库本身照常链接。
// 它是必要条件而非充分条件:两次核对之间完成的 grpc_init → grpc_shutdown、绕过 grpc_init 直接取官方 EventEngine
// 的代码,它都看不到;是否真的没有官方线程,仍以线程名 / thread baseline 核对为准(见上面的设计文档)。
//
// 头文件刻意不 include 任何 grpc 头:调用方不必看到 grpc 的 include 路径,依赖只收在 grpc_runtime_probe.cpp
// 一个翻译单元里。

namespace node {
namespace observability {

// 官方 gRPC 运行时当前是否已初始化(grpc_is_initialized() != 0)。
// 线程安全,任意线程可调;代价是一次 gRPC 内部互斥量加锁,适合低频采样,不要放进 per-tick 热路径。
// 进程内第一次调用会执行 gRPC 的一次性基础准备(建内部互斥量、读 GRPC_* 环境变量),它不建线程、
// 也不计入初始化次数,所以探针自身不会把结果变成 true。
[[nodiscard]] bool IsGrpcRuntimeInitialized();

} // namespace observability
} // namespace node
