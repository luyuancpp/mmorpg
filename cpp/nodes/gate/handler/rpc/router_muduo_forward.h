#pragma once

// gate → 路由服的 Forward 发送(docs/design/grpc-muduo-transport.md §8;第 3 批起是唯一实现,§14)。
//
// 对客户端的可见行为与生成客户端的全局处理器逐条相同:两条路径都只调 gate_rpc_reply_bridge(含路由 lane 的认领)。
//   成功 → 按响应初始 metadata 里回显的 x-session-detail-bin 解出会话,交成功桥;
//   失败 → 按发出的 SessionDetails 原始字节交失败桥;不重试。
// 通道由 NodeConnector 在服务发现时建好挂在路由服节点实体上(MuduoGrpcChannelPtr),这里不建连。

#include <string>

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

// 经 muduo_grpc 通道发一次 ClientRpcRouter.Forward。只能在 gate 的 EventLoop 线程调用。
// sessionDetailsBin 是序列化好的 SessionDetails(含路由 lane 的 gate_call_seq 等字段),原样作为
// x-session-detail-bin 的内容 —— 不解析、不重新序列化,路由服原值回写,lane 靠它认领回包。
void SendForward(muduo_grpc::Channel &channel, const client_rpc_router::ForwardRequest &request,
                 const std::string &sessionDetailsBin);

} // namespace gate_router_muduo
