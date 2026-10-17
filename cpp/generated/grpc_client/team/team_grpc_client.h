#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/team_service_metadata.h"

namespace teampb {
#pragma region ClientPlayerTeamCreateTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamCreateTeamGrpcPath = "/teampb.ClientPlayerTeam/CreateTeam";
using AsyncClientPlayerTeamCreateTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamCreateTeamHandlerFunctionType AsyncClientPlayerTeamCreateTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamCreateTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::CreateTeamRequest&)>;
extern AsyncClientPlayerTeamCreateTeamFailedHandlerFunctionType AsyncClientPlayerTeamCreateTeamFailedHandler;

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request);
void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamGetMyTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamGetMyTeamGrpcPath = "/teampb.ClientPlayerTeam/GetMyTeam";
using AsyncClientPlayerTeamGetMyTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamGetMyTeamHandlerFunctionType AsyncClientPlayerTeamGetMyTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamGetMyTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::GetMyTeamRequest&)>;
extern AsyncClientPlayerTeamGetMyTeamFailedHandlerFunctionType AsyncClientPlayerTeamGetMyTeamFailedHandler;

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request);
void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamApplyJoinTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamApplyJoinTeamGrpcPath = "/teampb.ClientPlayerTeam/ApplyJoinTeam";
using AsyncClientPlayerTeamApplyJoinTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamApplyJoinTeamHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamApplyJoinTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::ApplyJoinTeamRequest&)>;
extern AsyncClientPlayerTeamApplyJoinTeamFailedHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamFailedHandler;

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request);
void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamHandleApplication
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamHandleApplicationGrpcPath = "/teampb.ClientPlayerTeam/HandleApplication";
using AsyncClientPlayerTeamHandleApplicationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamHandleApplicationHandlerFunctionType AsyncClientPlayerTeamHandleApplicationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamHandleApplicationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::HandleApplicationRequest&)>;
extern AsyncClientPlayerTeamHandleApplicationFailedHandlerFunctionType AsyncClientPlayerTeamHandleApplicationFailedHandler;

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request);
void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamInviteToTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamInviteToTeamGrpcPath = "/teampb.ClientPlayerTeam/InviteToTeam";
using AsyncClientPlayerTeamInviteToTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamInviteToTeamHandlerFunctionType AsyncClientPlayerTeamInviteToTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamInviteToTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::InviteToTeamRequest&)>;
extern AsyncClientPlayerTeamInviteToTeamFailedHandlerFunctionType AsyncClientPlayerTeamInviteToTeamFailedHandler;

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request);
void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamRespondInvite
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamRespondInviteGrpcPath = "/teampb.ClientPlayerTeam/RespondInvite";
using AsyncClientPlayerTeamRespondInviteHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamRespondInviteHandlerFunctionType AsyncClientPlayerTeamRespondInviteHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamRespondInviteFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::RespondInviteRequest&)>;
extern AsyncClientPlayerTeamRespondInviteFailedHandlerFunctionType AsyncClientPlayerTeamRespondInviteFailedHandler;

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request);
void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamListMyInvites
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamListMyInvitesGrpcPath = "/teampb.ClientPlayerTeam/ListMyInvites";
using AsyncClientPlayerTeamListMyInvitesHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::ListMyInvitesResponse&)>;
extern AsyncClientPlayerTeamListMyInvitesHandlerFunctionType AsyncClientPlayerTeamListMyInvitesHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamListMyInvitesFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::ListMyInvitesRequest&)>;
extern AsyncClientPlayerTeamListMyInvitesFailedHandlerFunctionType AsyncClientPlayerTeamListMyInvitesFailedHandler;

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request);
void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request, GrpcDone<::teampb::ListMyInvitesResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamLeaveTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamLeaveTeamGrpcPath = "/teampb.ClientPlayerTeam/LeaveTeam";
using AsyncClientPlayerTeamLeaveTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamLeaveTeamHandlerFunctionType AsyncClientPlayerTeamLeaveTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamLeaveTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::LeaveTeamRequest&)>;
extern AsyncClientPlayerTeamLeaveTeamFailedHandlerFunctionType AsyncClientPlayerTeamLeaveTeamFailedHandler;

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request);
void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamKickMember
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamKickMemberGrpcPath = "/teampb.ClientPlayerTeam/KickMember";
using AsyncClientPlayerTeamKickMemberHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamKickMemberHandlerFunctionType AsyncClientPlayerTeamKickMemberHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamKickMemberFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::KickMemberRequest&)>;
extern AsyncClientPlayerTeamKickMemberFailedHandlerFunctionType AsyncClientPlayerTeamKickMemberFailedHandler;

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request);
void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamTransferLeader
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamTransferLeaderGrpcPath = "/teampb.ClientPlayerTeam/TransferLeader";
using AsyncClientPlayerTeamTransferLeaderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamTransferLeaderHandlerFunctionType AsyncClientPlayerTeamTransferLeaderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamTransferLeaderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::TransferLeaderRequest&)>;
extern AsyncClientPlayerTeamTransferLeaderFailedHandlerFunctionType AsyncClientPlayerTeamTransferLeaderFailedHandler;

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request);
void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamDisbandTeam
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamDisbandTeamGrpcPath = "/teampb.ClientPlayerTeam/DisbandTeam";
using AsyncClientPlayerTeamDisbandTeamHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamDisbandTeamHandlerFunctionType AsyncClientPlayerTeamDisbandTeamHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamDisbandTeamFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::DisbandTeamRequest&)>;
extern AsyncClientPlayerTeamDisbandTeamFailedHandlerFunctionType AsyncClientPlayerTeamDisbandTeamFailedHandler;

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request);
void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamStartTeamMatch
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamStartTeamMatchGrpcPath = "/teampb.ClientPlayerTeam/StartTeamMatch";
using AsyncClientPlayerTeamStartTeamMatchHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::teampb::TeamResponse&)>;
extern AsyncClientPlayerTeamStartTeamMatchHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamStartTeamMatchFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::StartTeamMatchRequest&)>;
extern AsyncClientPlayerTeamStartTeamMatchFailedHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchFailedHandler;

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request);
void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request, GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamSnapshot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamNotifyTeamSnapshotGrpcPath = "/teampb.ClientPlayerTeam/NotifyTeamSnapshot";
using AsyncClientPlayerTeamNotifyTeamSnapshotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncClientPlayerTeamNotifyTeamSnapshotHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::TeamSnapshotS2C&)>;
extern AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler;

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request);
void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamInvite
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamNotifyTeamInviteGrpcPath = "/teampb.ClientPlayerTeam/NotifyTeamInvite";
using AsyncClientPlayerTeamNotifyTeamInviteHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncClientPlayerTeamNotifyTeamInviteHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamNotifyTeamInviteFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::TeamInviteS2C&)>;
extern AsyncClientPlayerTeamNotifyTeamInviteFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteFailedHandler;

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request);
void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamEvent
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerTeamNotifyTeamEventGrpcPath = "/teampb.ClientPlayerTeam/NotifyTeamEvent";
using AsyncClientPlayerTeamNotifyTeamEventHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncClientPlayerTeamNotifyTeamEventHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerTeamNotifyTeamEventFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::teampb::TeamEventS2C&)>;
extern AsyncClientPlayerTeamNotifyTeamEventFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventFailedHandler;

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request);
void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetTeamHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetTeamIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetTeamFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetTeamIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetTeamCallDeadline(std::chrono::milliseconds deadline);

}// namespace teampb
