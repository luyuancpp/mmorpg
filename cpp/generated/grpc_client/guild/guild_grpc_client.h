#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/guild_service_metadata.h"

namespace guildpb {
#pragma region GuildServiceCreateGuild
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceCreateGuildGrpcPath = "/guildpb.GuildService/CreateGuild";
using AsyncGuildServiceCreateGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::CreateGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildGrpcPath = "/guildpb.GuildService/GetGuild";
using AsyncGuildServiceGetGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetPlayerGuildGrpcPath = "/guildpb.GuildService/GetPlayerGuild";
using AsyncGuildServiceGetPlayerGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetPlayerGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceLeaveGuildGrpcPath = "/guildpb.GuildService/LeaveGuild";
using AsyncGuildServiceLeaveGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::LeaveGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceDisbandGuildGrpcPath = "/guildpb.GuildService/DisbandGuild";
using AsyncGuildServiceDisbandGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::DisbandGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceSetAnnouncementGrpcPath = "/guildpb.GuildService/SetAnnouncement";
using AsyncGuildServiceSetAnnouncementHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::SetAnnouncementResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceSetGuildMemberRoleGrpcPath = "/guildpb.GuildService/SetGuildMemberRole";
using AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::SetGuildMemberRoleResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceKickGuildMemberGrpcPath = "/guildpb.GuildService/KickGuildMember";
using AsyncGuildServiceKickGuildMemberHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::KickGuildMemberResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceTransferGuildLeaderGrpcPath = "/guildpb.GuildService/TransferGuildLeader";
using AsyncGuildServiceTransferGuildLeaderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::TransferGuildLeaderResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceApplyJoinGuildGrpcPath = "/guildpb.GuildService/ApplyJoinGuild";
using AsyncGuildServiceApplyJoinGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::ApplyJoinGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceCancelGuildApplicationGrpcPath = "/guildpb.GuildService/CancelGuildApplication";
using AsyncGuildServiceCancelGuildApplicationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::CancelGuildApplicationResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceListMyGuildApplicationsGrpcPath = "/guildpb.GuildService/ListMyGuildApplications";
using AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::ListMyGuildApplicationsResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceListGuildApplicationsGrpcPath = "/guildpb.GuildService/ListGuildApplications";
using AsyncGuildServiceListGuildApplicationsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::ListGuildApplicationsResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceReviewGuildApplicationGrpcPath = "/guildpb.GuildService/ReviewGuildApplication";
using AsyncGuildServiceReviewGuildApplicationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::ReviewGuildApplicationResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceNotifyGuildChangedGrpcPath = "/guildpb.GuildService/NotifyGuildChanged";
using AsyncGuildServiceNotifyGuildChangedHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceUpdateGuildScoreGrpcPath = "/guildpb.GuildService/UpdateGuildScore";
using AsyncGuildServiceUpdateGuildScoreHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::UpdateGuildScoreResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildRankGrpcPath = "/guildpb.GuildService/GetGuildRank";
using AsyncGuildServiceGetGuildRankHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildRankResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildRankByGuildGrpcPath = "/guildpb.GuildService/GetGuildRankByGuild";
using AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildRankByGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildDonateOptionsGrpcPath = "/guildpb.GuildService/GetGuildDonateOptions";
using AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildDonateOptionsResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceDonateToGuildGrpcPath = "/guildpb.GuildService/DonateToGuild";
using AsyncGuildServiceDonateToGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::DonateToGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceUpgradeGuildGrpcPath = "/guildpb.GuildService/UpgradeGuild";
using AsyncGuildServiceUpgradeGuildHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::UpgradeGuildResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildShopGrpcPath = "/guildpb.GuildService/GetGuildShop";
using AsyncGuildServiceGetGuildShopHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildShopResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceBuyGuildShopGoodsGrpcPath = "/guildpb.GuildService/BuyGuildShopGoods";
using AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::BuyGuildShopGoodsResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceGetGuildActivitiesGrpcPath = "/guildpb.GuildService/GetGuildActivities";
using AsyncGuildServiceGetGuildActivitiesHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::GetGuildActivitiesResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceLightGuildLanternGrpcPath = "/guildpb.GuildService/LightGuildLantern";
using AsyncGuildServiceLightGuildLanternHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::LightGuildLanternResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceClaimGuildReunionGrpcPath = "/guildpb.GuildService/ClaimGuildReunion";
using AsyncGuildServiceClaimGuildReunionHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::ClaimGuildReunionResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceStartGuildTrialGrpcPath = "/guildpb.GuildService/StartGuildTrial";
using AsyncGuildServiceStartGuildTrialHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::StartGuildTrialResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kGuildServiceRespondGuildTrialInviteGrpcPath = "/guildpb.GuildService/RespondGuildTrialInvite";
using AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::guildpb::RespondGuildTrialInviteResponse&)>;
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
void SetGuildHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetGuildIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetGuildCallDeadline(std::chrono::milliseconds deadline);

}// namespace guildpb
