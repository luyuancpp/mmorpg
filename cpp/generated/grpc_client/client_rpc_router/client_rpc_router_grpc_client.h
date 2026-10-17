#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/client_rpc_router_service_metadata.h"

namespace client_rpc_router {
#pragma region ClientRpcRouterForward
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientRpcRouterForwardGrpcPath = "/client_rpc_router.ClientRpcRouter/Forward";
using AsyncClientRpcRouterForwardHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::MessageContent&)>;
extern AsyncClientRpcRouterForwardHandlerFunctionType AsyncClientRpcRouterForwardHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientRpcRouterForwardFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::client_rpc_router::ForwardRequest&)>;
extern AsyncClientRpcRouterForwardFailedHandlerFunctionType AsyncClientRpcRouterForwardFailedHandler;

void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request);
void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientRpcRouterForward(entt::registry& registry, entt::entity nodeEntity, const ::client_rpc_router::ForwardRequest& request, GrpcDone<::MessageContent> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetClientRpcRouterHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetClientRpcRouterIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetClientRpcRouterFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetClientRpcRouterIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetClientRpcRouterCallDeadline(std::chrono::milliseconds deadline);

}// namespace client_rpc_router
