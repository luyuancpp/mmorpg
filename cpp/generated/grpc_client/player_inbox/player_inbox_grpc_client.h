#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/player_inbox/player_inbox.grpc.pb.h"

#include "rpc/service_metadata/player_inbox_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace player_inbox {
using PlayerInboxStubPtr = std::unique_ptr<PlayerInbox::Stub>;
#pragma region PlayerInboxFetchItems

struct AsyncPlayerInboxFetchItemsGrpcClient {
    uint32_t messageId{ PlayerInboxFetchItemsMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::FetchPlayerInboxResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::FetchPlayerInboxResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::FetchPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxFetchItemsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::FetchPlayerInboxResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxFetchItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::FetchPlayerInboxResponse&)>;
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

struct AsyncPlayerInboxAckItemsGrpcClient {
    uint32_t messageId{ PlayerInboxAckItemsMessageId };
    ClientContext context;
    Status status;
    ::player_inbox::AckPlayerInboxResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::player_inbox::AckPlayerInboxResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::player_inbox::AckPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAckItemsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::player_inbox::AckPlayerInboxResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncPlayerInboxAckItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::AckPlayerInboxResponse&)>;
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
void SetPlayerInboxHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline);
void HandlePlayerInboxCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitPlayerInboxGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace player_inbox
