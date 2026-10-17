#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/player_inbox_admin_service_metadata.h"

namespace player_inbox {
#pragma region PlayerInboxAdminGmSanctionPlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminGmSanctionPlayerGrpcPath = "/player_inbox.PlayerInboxAdmin/GmSanctionPlayer";
using AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::GmSanctionPlayerResponse&)>;
extern AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminGmSanctionPlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::GmSanctionPlayerRequest&)>;
extern AsyncPlayerInboxAdminGmSanctionPlayerFailedHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler;

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request);
void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request, GrpcDone<::player_inbox::GmSanctionPlayerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAdminGetSummary
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminGetSummaryGrpcPath = "/player_inbox.PlayerInboxAdmin/GetSummary";
using AsyncPlayerInboxAdminGetSummaryHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::GetPlayerInboxSummaryResponse&)>;
extern AsyncPlayerInboxAdminGetSummaryHandlerFunctionType AsyncPlayerInboxAdminGetSummaryHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminGetSummaryFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::GetPlayerInboxSummaryRequest&)>;
extern AsyncPlayerInboxAdminGetSummaryFailedHandlerFunctionType AsyncPlayerInboxAdminGetSummaryFailedHandler;

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request);
void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request, GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAdminListItems
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminListItemsGrpcPath = "/player_inbox.PlayerInboxAdmin/ListItems";
using AsyncPlayerInboxAdminListItemsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::ListPlayerInboxItemsResponse&)>;
extern AsyncPlayerInboxAdminListItemsHandlerFunctionType AsyncPlayerInboxAdminListItemsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminListItemsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::ListPlayerInboxItemsRequest&)>;
extern AsyncPlayerInboxAdminListItemsFailedHandlerFunctionType AsyncPlayerInboxAdminListItemsFailedHandler;

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request);
void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request, GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAdminLookupByRequest
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminLookupByRequestGrpcPath = "/player_inbox.PlayerInboxAdmin/LookupByRequest";
using AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::LookupPlayerInboxResponse&)>;
extern AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminLookupByRequestFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::LookupPlayerInboxRequest&)>;
extern AsyncPlayerInboxAdminLookupByRequestFailedHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestFailedHandler;

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request);
void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request, GrpcDone<::player_inbox::LookupPlayerInboxResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAdminVoidItem
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminVoidItemGrpcPath = "/player_inbox.PlayerInboxAdmin/VoidItem";
using AsyncPlayerInboxAdminVoidItemHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::VoidPlayerInboxItemResponse&)>;
extern AsyncPlayerInboxAdminVoidItemHandlerFunctionType AsyncPlayerInboxAdminVoidItemHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminVoidItemFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::VoidPlayerInboxItemRequest&)>;
extern AsyncPlayerInboxAdminVoidItemFailedHandlerFunctionType AsyncPlayerInboxAdminVoidItemFailedHandler;

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request);
void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request, GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerInboxAdminReconcilePlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerInboxAdminReconcilePlayerGrpcPath = "/player_inbox.PlayerInboxAdmin/ReconcilePlayer";
using AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::player_inbox::ReconcilePlayerInboxResponse&)>;
extern AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerInboxAdminReconcilePlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::player_inbox::ReconcilePlayerInboxRequest&)>;
extern AsyncPlayerInboxAdminReconcilePlayerFailedHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerFailedHandler;

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request);
void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request, GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetPlayerInboxAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetPlayerInboxAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline);

}// namespace player_inbox
