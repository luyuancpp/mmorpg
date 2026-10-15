#pragma once

// gate → 路由服的 muduo_grpc 路径(GATE_ROUTER_TRANSPORT=muduo_h2,docs/design/grpc-muduo-transport.md §8)。
//
// 对客户端的可见行为与官方 gRPC 路径逐条相同:两条路径都只调 gate_rpc_reply_bridge(含路由 lane 的认领)。
//   成功 → 按响应初始 metadata 里回显的 x-session-detail-bin 解出会话,交成功桥;
//   失败 → 按发出的 SessionDetails 原始字节交失败桥;不重试,更不改走官方实现重发。

#include <string>

namespace muduo
{
namespace net
{
class EventLoop;
} // namespace net
} // namespace muduo

namespace muduo_grpc
{
class Channel;
} // namespace muduo_grpc

namespace client_rpc_router
{
class ForwardRequest;
} // namespace client_rpc_router

namespace gate_router_muduo
{

// 在 ClientRpcRouterNodeService 的节点 registry 上下文里装通道工厂:此后发现的路由服节点只建
// muduo_grpc::Channel。必须在服务发现开始之前调用(gate main 的配置回调里)。
void InstallChannelFactory(muduo::net::EventLoop &loop);

// 经 muduo_grpc 通道发一次 ClientRpcRouter.Forward。只能在 gate 的 EventLoop 线程调用。
// sessionDetailsBin 是序列化好的 SessionDetails(含路由 lane 的 gate_call_seq 等字段),原样作为
// x-session-detail-bin 的内容 —— 不解析、不重新序列化,路由服原值回写,lane 靠它认领回包。
void SendForward(muduo_grpc::Channel &channel, const client_rpc_router::ForwardRequest &request,
                 const std::string &sessionDetailsBin);

} // namespace gate_router_muduo
