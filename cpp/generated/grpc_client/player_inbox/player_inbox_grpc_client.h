#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/player_inbox_service_metadata.h"

namespace player_inbox {
#pragma region PlayerInboxFetchItems
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxFetchItemsGrpcPath = "/player_inbox.PlayerInbox/FetchItems";
using AsyncPlayerInboxFetchItemsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::FetchPlayerInboxResponse&)>;
extern AsyncPlayerInboxFetchItemsHandlerFunctionType AsyncPlayerInboxFetchItemsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxFetchItemsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::FetchPlayerInboxRequest&)>;
extern AsyncPlayerInboxFetchItemsFailedHandlerFunctionType AsyncPlayerInboxFetchItemsFailedHandler;

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request);
void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request, GrpcDone<::player_inbox::FetchPlayerInboxResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAckItems
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAckItemsGrpcPath = "/player_inbox.PlayerInbox/AckItems";
using AsyncPlayerInboxAckItemsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::AckPlayerInboxResponse&)>;
extern AsyncPlayerInboxAckItemsHandlerFunctionType AsyncPlayerInboxAckItemsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAckItemsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::AckPlayerInboxRequest&)>;
extern AsyncPlayerInboxAckItemsFailedHandlerFunctionType AsyncPlayerInboxAckItemsFailedHandler;

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request);
void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request, GrpcDone<::player_inbox::AckPlayerInboxResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetPlayerInboxHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline);

}// namespace player_inbox
