#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/guild/guild_internal.grpc.pb.h"

#include "rpc/service_metadata/guild_internal_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace guildpb {
using GuildInternalStubPtr = std::unique_ptr<GuildInternal::Stub>;
#pragma region GuildInternalListAppliedAssetOpsSince

struct AsyncGuildInternalListAppliedAssetOpsSinceGrpcClient {
    uint32_t messageId{ GuildInternalListAppliedAssetOpsSinceMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ListAppliedAssetOpsSinceResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ListAppliedAssetOpsSinceResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ListAppliedAssetOpsSinceRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildInternalListAppliedAssetOpsSinceHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildInternalListAppliedAssetOpsSinceHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListAppliedAssetOpsSinceResponse&)>;
extern AsyncGuildInternalListAppliedAssetOpsSinceHandlerFunctionType AsyncGuildInternalListAppliedAssetOpsSinceHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildInternalListAppliedAssetOpsSinceFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ListAppliedAssetOpsSinceRequest&)>;
extern AsyncGuildInternalListAppliedAssetOpsSinceFailedHandlerFunctionType AsyncGuildInternalListAppliedAssetOpsSinceFailedHandler;

void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request);
void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildInternalListAppliedAssetOpsSince(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListAppliedAssetOpsSinceRequest& request, GrpcDone<::guildpb::ListAppliedAssetOpsSinceResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetGuildInternalHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildInternalIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildInternalFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetGuildInternalIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetGuildInternalCallDeadline(std::chrono::milliseconds deadline);
void HandleGuildInternalCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitGuildInternalGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace guildpb
