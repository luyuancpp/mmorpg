#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/guild/guild.grpc.pb.h"

#include "rpc/service_metadata/guild_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace guildpb {
using GuildServiceStubPtr = std::unique_ptr<GuildService::Stub>;
#pragma region GuildServiceCreateGuild

struct AsyncGuildServiceCreateGuildGrpcClient {
    uint32_t messageId{ GuildServiceCreateGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::CreateGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::CreateGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::CreateGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceCreateGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::CreateGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceCreateGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::CreateGuildResponse&)>;
extern AsyncGuildServiceCreateGuildHandlerFunctionType AsyncGuildServiceCreateGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceCreateGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::CreateGuildRequest&)>;
extern AsyncGuildServiceCreateGuildFailedHandlerFunctionType AsyncGuildServiceCreateGuildFailedHandler;

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request);
void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request, GrpcDone<::guildpb::CreateGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuild

struct AsyncGuildServiceGetGuildGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildResponse&)>;
extern AsyncGuildServiceGetGuildHandlerFunctionType AsyncGuildServiceGetGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildRequest&)>;
extern AsyncGuildServiceGetGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildFailedHandler;

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request);
void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request, GrpcDone<::guildpb::GetGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetPlayerGuild

struct AsyncGuildServiceGetPlayerGuildGrpcClient {
    uint32_t messageId{ GuildServiceGetPlayerGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetPlayerGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetPlayerGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetPlayerGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetPlayerGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetPlayerGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetPlayerGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetPlayerGuildResponse&)>;
extern AsyncGuildServiceGetPlayerGuildHandlerFunctionType AsyncGuildServiceGetPlayerGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetPlayerGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetPlayerGuildRequest&)>;
extern AsyncGuildServiceGetPlayerGuildFailedHandlerFunctionType AsyncGuildServiceGetPlayerGuildFailedHandler;

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request);
void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request, GrpcDone<::guildpb::GetPlayerGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceLeaveGuild

struct AsyncGuildServiceLeaveGuildGrpcClient {
    uint32_t messageId{ GuildServiceLeaveGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::LeaveGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::LeaveGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::LeaveGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceLeaveGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::LeaveGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceLeaveGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::LeaveGuildResponse&)>;
extern AsyncGuildServiceLeaveGuildHandlerFunctionType AsyncGuildServiceLeaveGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceLeaveGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::LeaveGuildRequest&)>;
extern AsyncGuildServiceLeaveGuildFailedHandlerFunctionType AsyncGuildServiceLeaveGuildFailedHandler;

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request);
void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request, GrpcDone<::guildpb::LeaveGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceDisbandGuild

struct AsyncGuildServiceDisbandGuildGrpcClient {
    uint32_t messageId{ GuildServiceDisbandGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::DisbandGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::DisbandGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::DisbandGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceDisbandGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::DisbandGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceDisbandGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::DisbandGuildResponse&)>;
extern AsyncGuildServiceDisbandGuildHandlerFunctionType AsyncGuildServiceDisbandGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceDisbandGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::DisbandGuildRequest&)>;
extern AsyncGuildServiceDisbandGuildFailedHandlerFunctionType AsyncGuildServiceDisbandGuildFailedHandler;

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request);
void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request, GrpcDone<::guildpb::DisbandGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceSetAnnouncement

struct AsyncGuildServiceSetAnnouncementGrpcClient {
    uint32_t messageId{ GuildServiceSetAnnouncementMessageId };
    ClientContext context;
    Status status;
    ::guildpb::SetAnnouncementResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::SetAnnouncementResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::SetAnnouncementRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceSetAnnouncementHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::SetAnnouncementResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceSetAnnouncementHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::SetAnnouncementResponse&)>;
extern AsyncGuildServiceSetAnnouncementHandlerFunctionType AsyncGuildServiceSetAnnouncementHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceSetAnnouncementFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::SetAnnouncementRequest&)>;
extern AsyncGuildServiceSetAnnouncementFailedHandlerFunctionType AsyncGuildServiceSetAnnouncementFailedHandler;

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request);
void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request, GrpcDone<::guildpb::SetAnnouncementResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceSetGuildMemberRole

struct AsyncGuildServiceSetGuildMemberRoleGrpcClient {
    uint32_t messageId{ GuildServiceSetGuildMemberRoleMessageId };
    ClientContext context;
    Status status;
    ::guildpb::SetGuildMemberRoleResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::SetGuildMemberRoleResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::SetGuildMemberRoleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceSetGuildMemberRoleHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::SetGuildMemberRoleResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::SetGuildMemberRoleResponse&)>;
extern AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceSetGuildMemberRoleFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::SetGuildMemberRoleRequest&)>;
extern AsyncGuildServiceSetGuildMemberRoleFailedHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleFailedHandler;

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request);
void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request, GrpcDone<::guildpb::SetGuildMemberRoleResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceKickGuildMember

struct AsyncGuildServiceKickGuildMemberGrpcClient {
    uint32_t messageId{ GuildServiceKickGuildMemberMessageId };
    ClientContext context;
    Status status;
    ::guildpb::KickGuildMemberResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::KickGuildMemberResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::KickGuildMemberRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceKickGuildMemberHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::KickGuildMemberResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceKickGuildMemberHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::KickGuildMemberResponse&)>;
extern AsyncGuildServiceKickGuildMemberHandlerFunctionType AsyncGuildServiceKickGuildMemberHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceKickGuildMemberFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::KickGuildMemberRequest&)>;
extern AsyncGuildServiceKickGuildMemberFailedHandlerFunctionType AsyncGuildServiceKickGuildMemberFailedHandler;

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request);
void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request, GrpcDone<::guildpb::KickGuildMemberResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceTransferGuildLeader

struct AsyncGuildServiceTransferGuildLeaderGrpcClient {
    uint32_t messageId{ GuildServiceTransferGuildLeaderMessageId };
    ClientContext context;
    Status status;
    ::guildpb::TransferGuildLeaderResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::TransferGuildLeaderResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::TransferGuildLeaderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceTransferGuildLeaderHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::TransferGuildLeaderResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceTransferGuildLeaderHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::TransferGuildLeaderResponse&)>;
extern AsyncGuildServiceTransferGuildLeaderHandlerFunctionType AsyncGuildServiceTransferGuildLeaderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceTransferGuildLeaderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::TransferGuildLeaderRequest&)>;
extern AsyncGuildServiceTransferGuildLeaderFailedHandlerFunctionType AsyncGuildServiceTransferGuildLeaderFailedHandler;

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request);
void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request, GrpcDone<::guildpb::TransferGuildLeaderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceApplyJoinGuild

struct AsyncGuildServiceApplyJoinGuildGrpcClient {
    uint32_t messageId{ GuildServiceApplyJoinGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ApplyJoinGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ApplyJoinGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ApplyJoinGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceApplyJoinGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ApplyJoinGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceApplyJoinGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ApplyJoinGuildResponse&)>;
extern AsyncGuildServiceApplyJoinGuildHandlerFunctionType AsyncGuildServiceApplyJoinGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceApplyJoinGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ApplyJoinGuildRequest&)>;
extern AsyncGuildServiceApplyJoinGuildFailedHandlerFunctionType AsyncGuildServiceApplyJoinGuildFailedHandler;

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request);
void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request, GrpcDone<::guildpb::ApplyJoinGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceCancelGuildApplication

struct AsyncGuildServiceCancelGuildApplicationGrpcClient {
    uint32_t messageId{ GuildServiceCancelGuildApplicationMessageId };
    ClientContext context;
    Status status;
    ::guildpb::CancelGuildApplicationResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::CancelGuildApplicationResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::CancelGuildApplicationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceCancelGuildApplicationHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::CancelGuildApplicationResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceCancelGuildApplicationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::CancelGuildApplicationResponse&)>;
extern AsyncGuildServiceCancelGuildApplicationHandlerFunctionType AsyncGuildServiceCancelGuildApplicationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceCancelGuildApplicationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::CancelGuildApplicationRequest&)>;
extern AsyncGuildServiceCancelGuildApplicationFailedHandlerFunctionType AsyncGuildServiceCancelGuildApplicationFailedHandler;

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request);
void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request, GrpcDone<::guildpb::CancelGuildApplicationResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceListMyGuildApplications

struct AsyncGuildServiceListMyGuildApplicationsGrpcClient {
    uint32_t messageId{ GuildServiceListMyGuildApplicationsMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ListMyGuildApplicationsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ListMyGuildApplicationsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ListMyGuildApplicationsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceListMyGuildApplicationsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListMyGuildApplicationsResponse&)>;
extern AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceListMyGuildApplicationsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ListMyGuildApplicationsRequest&)>;
extern AsyncGuildServiceListMyGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsFailedHandler;

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request);
void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request, GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceListGuildApplications

struct AsyncGuildServiceListGuildApplicationsGrpcClient {
    uint32_t messageId{ GuildServiceListGuildApplicationsMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ListGuildApplicationsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ListGuildApplicationsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ListGuildApplicationsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceListGuildApplicationsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ListGuildApplicationsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceListGuildApplicationsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListGuildApplicationsResponse&)>;
extern AsyncGuildServiceListGuildApplicationsHandlerFunctionType AsyncGuildServiceListGuildApplicationsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceListGuildApplicationsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ListGuildApplicationsRequest&)>;
extern AsyncGuildServiceListGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListGuildApplicationsFailedHandler;

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request);
void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request, GrpcDone<::guildpb::ListGuildApplicationsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceReviewGuildApplication

struct AsyncGuildServiceReviewGuildApplicationGrpcClient {
    uint32_t messageId{ GuildServiceReviewGuildApplicationMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ReviewGuildApplicationResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ReviewGuildApplicationResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ReviewGuildApplicationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceReviewGuildApplicationHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ReviewGuildApplicationResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceReviewGuildApplicationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ReviewGuildApplicationResponse&)>;
extern AsyncGuildServiceReviewGuildApplicationHandlerFunctionType AsyncGuildServiceReviewGuildApplicationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceReviewGuildApplicationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ReviewGuildApplicationRequest&)>;
extern AsyncGuildServiceReviewGuildApplicationFailedHandlerFunctionType AsyncGuildServiceReviewGuildApplicationFailedHandler;

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request);
void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request, GrpcDone<::guildpb::ReviewGuildApplicationResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceNotifyGuildChanged

struct AsyncGuildServiceNotifyGuildChangedGrpcClient {
    uint32_t messageId{ GuildServiceNotifyGuildChangedMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GuildChangedS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceNotifyGuildChangedHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceNotifyGuildChangedHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncGuildServiceNotifyGuildChangedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceNotifyGuildChangedFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GuildChangedS2C&)>;
extern AsyncGuildServiceNotifyGuildChangedFailedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedFailedHandler;

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request);
void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceUpdateGuildScore

struct AsyncGuildServiceUpdateGuildScoreGrpcClient {
    uint32_t messageId{ GuildServiceUpdateGuildScoreMessageId };
    ClientContext context;
    Status status;
    ::guildpb::UpdateGuildScoreResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::UpdateGuildScoreResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::UpdateGuildScoreRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceUpdateGuildScoreHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::UpdateGuildScoreResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceUpdateGuildScoreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::UpdateGuildScoreResponse&)>;
extern AsyncGuildServiceUpdateGuildScoreHandlerFunctionType AsyncGuildServiceUpdateGuildScoreHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceUpdateGuildScoreFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::UpdateGuildScoreRequest&)>;
extern AsyncGuildServiceUpdateGuildScoreFailedHandlerFunctionType AsyncGuildServiceUpdateGuildScoreFailedHandler;

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request);
void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request, GrpcDone<::guildpb::UpdateGuildScoreResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuildRank

struct AsyncGuildServiceGetGuildRankGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildRankMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildRankResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildRankResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildRankRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildRankHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildRankResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildRankHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildRankResponse&)>;
extern AsyncGuildServiceGetGuildRankHandlerFunctionType AsyncGuildServiceGetGuildRankHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildRankFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildRankRequest&)>;
extern AsyncGuildServiceGetGuildRankFailedHandlerFunctionType AsyncGuildServiceGetGuildRankFailedHandler;

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request);
void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request, GrpcDone<::guildpb::GetGuildRankResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuildRankByGuild

struct AsyncGuildServiceGetGuildRankByGuildGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildRankByGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildRankByGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildRankByGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildRankByGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildRankByGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildRankByGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildRankByGuildResponse&)>;
extern AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildRankByGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildRankByGuildRequest&)>;
extern AsyncGuildServiceGetGuildRankByGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildFailedHandler;

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request);
void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request, GrpcDone<::guildpb::GetGuildRankByGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuildDonateOptions

struct AsyncGuildServiceGetGuildDonateOptionsGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildDonateOptionsMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildDonateOptionsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildDonateOptionsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildDonateOptionsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildDonateOptionsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildDonateOptionsResponse&)>;
extern AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildDonateOptionsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildDonateOptionsRequest&)>;
extern AsyncGuildServiceGetGuildDonateOptionsFailedHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsFailedHandler;

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request);
void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request, GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceDonateToGuild

struct AsyncGuildServiceDonateToGuildGrpcClient {
    uint32_t messageId{ GuildServiceDonateToGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::DonateToGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::DonateToGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::DonateToGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceDonateToGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::DonateToGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceDonateToGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::DonateToGuildResponse&)>;
extern AsyncGuildServiceDonateToGuildHandlerFunctionType AsyncGuildServiceDonateToGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceDonateToGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::DonateToGuildRequest&)>;
extern AsyncGuildServiceDonateToGuildFailedHandlerFunctionType AsyncGuildServiceDonateToGuildFailedHandler;

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request);
void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request, GrpcDone<::guildpb::DonateToGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceUpgradeGuild

struct AsyncGuildServiceUpgradeGuildGrpcClient {
    uint32_t messageId{ GuildServiceUpgradeGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::UpgradeGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::UpgradeGuildResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::UpgradeGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceUpgradeGuildHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::UpgradeGuildResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceUpgradeGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::UpgradeGuildResponse&)>;
extern AsyncGuildServiceUpgradeGuildHandlerFunctionType AsyncGuildServiceUpgradeGuildHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceUpgradeGuildFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::UpgradeGuildRequest&)>;
extern AsyncGuildServiceUpgradeGuildFailedHandlerFunctionType AsyncGuildServiceUpgradeGuildFailedHandler;

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request);
void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request, GrpcDone<::guildpb::UpgradeGuildResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuildShop

struct AsyncGuildServiceGetGuildShopGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildShopMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildShopResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildShopResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildShopRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildShopHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildShopResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildShopHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildShopResponse&)>;
extern AsyncGuildServiceGetGuildShopHandlerFunctionType AsyncGuildServiceGetGuildShopHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildShopFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildShopRequest&)>;
extern AsyncGuildServiceGetGuildShopFailedHandlerFunctionType AsyncGuildServiceGetGuildShopFailedHandler;

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request);
void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request, GrpcDone<::guildpb::GetGuildShopResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceBuyGuildShopGoods

struct AsyncGuildServiceBuyGuildShopGoodsGrpcClient {
    uint32_t messageId{ GuildServiceBuyGuildShopGoodsMessageId };
    ClientContext context;
    Status status;
    ::guildpb::BuyGuildShopGoodsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::BuyGuildShopGoodsResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::BuyGuildShopGoodsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceBuyGuildShopGoodsHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::BuyGuildShopGoodsResponse&)>;
extern AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceBuyGuildShopGoodsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::BuyGuildShopGoodsRequest&)>;
extern AsyncGuildServiceBuyGuildShopGoodsFailedHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsFailedHandler;

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request);
void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request, GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceGetGuildActivities

struct AsyncGuildServiceGetGuildActivitiesGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildActivitiesMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildActivitiesResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildActivitiesResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::GetGuildActivitiesRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildActivitiesHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::GetGuildActivitiesResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceGetGuildActivitiesHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildActivitiesResponse&)>;
extern AsyncGuildServiceGetGuildActivitiesHandlerFunctionType AsyncGuildServiceGetGuildActivitiesHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceGetGuildActivitiesFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::GetGuildActivitiesRequest&)>;
extern AsyncGuildServiceGetGuildActivitiesFailedHandlerFunctionType AsyncGuildServiceGetGuildActivitiesFailedHandler;

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request);
void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request, GrpcDone<::guildpb::GetGuildActivitiesResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceLightGuildLantern

struct AsyncGuildServiceLightGuildLanternGrpcClient {
    uint32_t messageId{ GuildServiceLightGuildLanternMessageId };
    ClientContext context;
    Status status;
    ::guildpb::LightGuildLanternResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::LightGuildLanternResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::LightGuildLanternRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceLightGuildLanternHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::LightGuildLanternResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceLightGuildLanternHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::LightGuildLanternResponse&)>;
extern AsyncGuildServiceLightGuildLanternHandlerFunctionType AsyncGuildServiceLightGuildLanternHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceLightGuildLanternFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::LightGuildLanternRequest&)>;
extern AsyncGuildServiceLightGuildLanternFailedHandlerFunctionType AsyncGuildServiceLightGuildLanternFailedHandler;

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request);
void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request, GrpcDone<::guildpb::LightGuildLanternResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceClaimGuildReunion

struct AsyncGuildServiceClaimGuildReunionGrpcClient {
    uint32_t messageId{ GuildServiceClaimGuildReunionMessageId };
    ClientContext context;
    Status status;
    ::guildpb::ClaimGuildReunionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::ClaimGuildReunionResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::ClaimGuildReunionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceClaimGuildReunionHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::ClaimGuildReunionResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceClaimGuildReunionHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ClaimGuildReunionResponse&)>;
extern AsyncGuildServiceClaimGuildReunionHandlerFunctionType AsyncGuildServiceClaimGuildReunionHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceClaimGuildReunionFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::ClaimGuildReunionRequest&)>;
extern AsyncGuildServiceClaimGuildReunionFailedHandlerFunctionType AsyncGuildServiceClaimGuildReunionFailedHandler;

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request);
void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request, GrpcDone<::guildpb::ClaimGuildReunionResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceStartGuildTrial

struct AsyncGuildServiceStartGuildTrialGrpcClient {
    uint32_t messageId{ GuildServiceStartGuildTrialMessageId };
    ClientContext context;
    Status status;
    ::guildpb::StartGuildTrialResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::StartGuildTrialResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::StartGuildTrialRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceStartGuildTrialHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::StartGuildTrialResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceStartGuildTrialHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::StartGuildTrialResponse&)>;
extern AsyncGuildServiceStartGuildTrialHandlerFunctionType AsyncGuildServiceStartGuildTrialHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceStartGuildTrialFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::StartGuildTrialRequest&)>;
extern AsyncGuildServiceStartGuildTrialFailedHandlerFunctionType AsyncGuildServiceStartGuildTrialFailedHandler;

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request);
void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request, GrpcDone<::guildpb::StartGuildTrialResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region GuildServiceRespondGuildTrialInvite

struct AsyncGuildServiceRespondGuildTrialInviteGrpcClient {
    uint32_t messageId{ GuildServiceRespondGuildTrialInviteMessageId };
    ClientContext context;
    Status status;
    ::guildpb::RespondGuildTrialInviteResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::RespondGuildTrialInviteResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::guildpb::RespondGuildTrialInviteRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceRespondGuildTrialInviteHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::RespondGuildTrialInviteResponse&)>;
extern AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncGuildServiceRespondGuildTrialInviteFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::guildpb::RespondGuildTrialInviteRequest&)>;
extern AsyncGuildServiceRespondGuildTrialInviteFailedHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteFailedHandler;

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request);
void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request, GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetGuildHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetGuildIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetGuildCallDeadline(std::chrono::milliseconds deadline);
void HandleGuildCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitGuildGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace guildpb
