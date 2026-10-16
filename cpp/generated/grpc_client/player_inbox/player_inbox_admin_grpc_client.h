#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/player_inbox/player_inbox_admin.grpc.pb.h"

#include "rpc/service_metadata/player_inbox_admin_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace player_inbox {
using PlayerInboxAdminStubPtr = std::unique_ptr<PlayerInboxAdmin::Stub>;
#pragma region PlayerInboxAdminGmSanctionPlayer

struct AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient {
    uint32_t messageId{ PlayerInboxAdminGmSanctionPlayerMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::GmSanctionPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::GmSanctionPlayerResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::GmSanctionPlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminGmSanctionPlayerHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::GmSanctionPlayerResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::GmSanctionPlayerResponse&)>;
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

struct AsyncPlayerInboxAdminGetSummaryGrpcClient {
    uint32_t messageId{ PlayerInboxAdminGetSummaryMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::GetPlayerInboxSummaryResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::GetPlayerInboxSummaryResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::GetPlayerInboxSummaryRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminGetSummaryHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminGetSummaryHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::GetPlayerInboxSummaryResponse&)>;
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

struct AsyncPlayerInboxAdminListItemsGrpcClient {
    uint32_t messageId{ PlayerInboxAdminListItemsMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::ListPlayerInboxItemsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::ListPlayerInboxItemsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::ListPlayerInboxItemsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminListItemsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminListItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::ListPlayerInboxItemsResponse&)>;
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

struct AsyncPlayerInboxAdminLookupByRequestGrpcClient {
    uint32_t messageId{ PlayerInboxAdminLookupByRequestMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::LookupPlayerInboxResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::LookupPlayerInboxResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::LookupPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminLookupByRequestHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::LookupPlayerInboxResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::LookupPlayerInboxResponse&)>;
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

struct AsyncPlayerInboxAdminVoidItemGrpcClient {
    uint32_t messageId{ PlayerInboxAdminVoidItemMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::VoidPlayerInboxItemResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::VoidPlayerInboxItemResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::VoidPlayerInboxItemRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminVoidItemHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminVoidItemHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::VoidPlayerInboxItemResponse&)>;
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

struct AsyncPlayerInboxAdminReconcilePlayerGrpcClient {
    uint32_t messageId{ PlayerInboxAdminReconcilePlayerMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::ReconcilePlayerInboxResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::ReconcilePlayerInboxResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::ReconcilePlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminReconcilePlayerHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::ReconcilePlayerInboxResponse&)>;
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
void SetPlayerInboxAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetPlayerInboxAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline);
void HandlePlayerInboxAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitPlayerInboxAdminGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace player_inbox
