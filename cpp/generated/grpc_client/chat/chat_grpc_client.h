#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/chat_service_metadata.h"

namespace chatpb {
#pragma region ClientPlayerChatSendChat
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerChatSendChatGrpcPath = "/chatpb.ClientPlayerChat/SendChat";
using AsyncClientPlayerChatSendChatHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::chatpb::SendChatResponse&)>;
extern AsyncClientPlayerChatSendChatHandlerFunctionType AsyncClientPlayerChatSendChatHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerChatSendChatFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::chatpb::SendChatRequest&)>;
extern AsyncClientPlayerChatSendChatFailedHandlerFunctionType AsyncClientPlayerChatSendChatFailedHandler;

void SendClientPlayerChatSendChat(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::SendChatRequest& request);
void SendClientPlayerChatSendChat(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::SendChatRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerChatSendChat(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerChatSendChat(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::SendChatRequest& request, GrpcDone<::chatpb::SendChatResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerChatPullChatHistory
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerChatPullChatHistoryGrpcPath = "/chatpb.ClientPlayerChat/PullChatHistory";
using AsyncClientPlayerChatPullChatHistoryHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::chatpb::PullChatHistoryResponse&)>;
extern AsyncClientPlayerChatPullChatHistoryHandlerFunctionType AsyncClientPlayerChatPullChatHistoryHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerChatPullChatHistoryFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::chatpb::PullChatHistoryRequest&)>;
extern AsyncClientPlayerChatPullChatHistoryFailedHandlerFunctionType AsyncClientPlayerChatPullChatHistoryFailedHandler;

void SendClientPlayerChatPullChatHistory(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::PullChatHistoryRequest& request);
void SendClientPlayerChatPullChatHistory(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::PullChatHistoryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerChatPullChatHistory(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerChatPullChatHistory(entt::registry& registry, entt::entity nodeEntity, const ::chatpb::PullChatHistoryRequest& request, GrpcDone<::chatpb::PullChatHistoryResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetChatHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetChatIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetChatFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetChatIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetChatCallDeadline(std::chrono::milliseconds deadline);

}// namespace chatpb
