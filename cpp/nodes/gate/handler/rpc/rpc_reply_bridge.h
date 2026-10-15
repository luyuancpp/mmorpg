#pragma once

// 出站 gRPC 调用的结果怎样落到客户端 —— gate 唯一的一份事实源。
//
// 官方 gRPC 桥(main.cpp 的 SetIfEmptyHandler / SetIfEmptyFailedHandler)与 muduo_grpc 路由路径
// (router_muduo_forward.cpp)都只调这里,所以两种传输对客户端的可见行为逐条相同,路由 lane 的保序认领
// (client_request_lane.h,docs/design/client-request-ordering.md §4.5)也对两种传输同样生效。
// 参见 docs/design/grpc-muduo-transport.md §8、docs/design/grpc-client-deadline-failure-callback.md §5。
//
// 只在 gate 的 EventLoop 线程调用(会话表是 thread_local)。

#include <string>

class SessionDetails;

namespace google::protobuf
{
class Message;
}

namespace gate_rpc_reply_bridge
{

// 成功桥。sessionDetails 是服务端在响应头里回写的会话(路由服原值回显;直连模式下 login 重新 Marshal)。
//   1. 路由 lane 的调用(gate_call_seq != 0)交给 lane 认领:核对代次与队头、恰好一次下发、放行下一条;
//   2. 否则按 session_id 找会话:MessageContent 原样下发,其它响应类型按 gRpcMethodRegistry 反查 message_id 包一层;
//      会话已不在、连接已释放就静默丢弃(断线通知的回包本来就没有会话可回)。
void DeliverReply(const SessionDetails &sessionDetails, const ::google::protobuf::Message &reply);

// 失败桥:调用失败 = 结果未知(下游可能已执行)。rawSessionDetails 是**发出时**的 SessionDetails 原始字节
// (未 base64);为空指针表示这次调用没带会话 metadata(如 etcd),只记 ERROR。
//   1. 失败日志按 1024 条采样(下游整体不可用时失败频率 = 在线玩家的 gRPC 消息频率);
//   2. 路由 lane 的调用交给 lane 认领(回带请求 id 的 1003,放行下一条);
//   3. 否则找会话、回 kServiceUnavailable;不按状态码区分 tip,不重试。
void NotifyCallFailure(const char *method, int statusCode, const std::string &statusMessage,
                       const std::string *rawSessionDetails);

} // namespace gate_rpc_reply_bridge
