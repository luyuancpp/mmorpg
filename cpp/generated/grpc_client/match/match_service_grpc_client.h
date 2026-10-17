#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/match_service_service_metadata.h"

namespace match {
#pragma region MatchServiceGetNetworkProbePlan
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceGetNetworkProbePlanGrpcPath = "/match.MatchService/GetNetworkProbePlan";
using AsyncMatchServiceGetNetworkProbePlanHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::GetNetworkProbePlanResponse&)>;
extern AsyncMatchServiceGetNetworkProbePlanHandlerFunctionType AsyncMatchServiceGetNetworkProbePlanHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceGetNetworkProbePlanFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::GetNetworkProbePlanRequest&)>;
extern AsyncMatchServiceGetNetworkProbePlanFailedHandlerFunctionType AsyncMatchServiceGetNetworkProbePlanFailedHandler;

void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request);
void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request, GrpcDone<::match::GetNetworkProbePlanResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceCommitNetworkProbe
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceCommitNetworkProbeGrpcPath = "/match.MatchService/CommitNetworkProbe";
using AsyncMatchServiceCommitNetworkProbeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::CommitNetworkProbeResponse&)>;
extern AsyncMatchServiceCommitNetworkProbeHandlerFunctionType AsyncMatchServiceCommitNetworkProbeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceCommitNetworkProbeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::CommitNetworkProbeRequest&)>;
extern AsyncMatchServiceCommitNetworkProbeFailedHandlerFunctionType AsyncMatchServiceCommitNetworkProbeFailedHandler;

void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request);
void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request, GrpcDone<::match::CommitNetworkProbeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceJoinQueue
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceJoinQueueGrpcPath = "/match.MatchService/JoinQueue";
using AsyncMatchServiceJoinQueueHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::JoinQueueResponse&)>;
extern AsyncMatchServiceJoinQueueHandlerFunctionType AsyncMatchServiceJoinQueueHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceJoinQueueFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::JoinQueueRequest&)>;
extern AsyncMatchServiceJoinQueueFailedHandlerFunctionType AsyncMatchServiceJoinQueueFailedHandler;

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request);
void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request, GrpcDone<::match::JoinQueueResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceCancelQueue
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceCancelQueueGrpcPath = "/match.MatchService/CancelQueue";
using AsyncMatchServiceCancelQueueHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncMatchServiceCancelQueueHandlerFunctionType AsyncMatchServiceCancelQueueHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceCancelQueueFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::CancelQueueRequest&)>;
extern AsyncMatchServiceCancelQueueFailedHandlerFunctionType AsyncMatchServiceCancelQueueFailedHandler;

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request);
void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceGetQueueStatus
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceGetQueueStatusGrpcPath = "/match.MatchService/GetQueueStatus";
using AsyncMatchServiceGetQueueStatusHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::GetQueueStatusResponse&)>;
extern AsyncMatchServiceGetQueueStatusHandlerFunctionType AsyncMatchServiceGetQueueStatusHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceGetQueueStatusFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::GetQueueStatusRequest&)>;
extern AsyncMatchServiceGetQueueStatusFailedHandlerFunctionType AsyncMatchServiceGetQueueStatusFailedHandler;

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request);
void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request, GrpcDone<::match::GetQueueStatusResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceChallengePlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceChallengePlayerGrpcPath = "/match.MatchService/ChallengePlayer";
using AsyncMatchServiceChallengePlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::ChallengePlayerResponse&)>;
extern AsyncMatchServiceChallengePlayerHandlerFunctionType AsyncMatchServiceChallengePlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceChallengePlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::ChallengePlayerRequest&)>;
extern AsyncMatchServiceChallengePlayerFailedHandlerFunctionType AsyncMatchServiceChallengePlayerFailedHandler;

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request);
void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request, GrpcDone<::match::ChallengePlayerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceRespondChallenge
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceRespondChallengeGrpcPath = "/match.MatchService/RespondChallenge";
using AsyncMatchServiceRespondChallengeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::RespondChallengeResponse&)>;
extern AsyncMatchServiceRespondChallengeHandlerFunctionType AsyncMatchServiceRespondChallengeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceRespondChallengeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::RespondChallengeRequest&)>;
extern AsyncMatchServiceRespondChallengeFailedHandlerFunctionType AsyncMatchServiceRespondChallengeFailedHandler;

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request);
void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request, GrpcDone<::match::RespondChallengeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceNotifyChallengeInvite
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceNotifyChallengeInviteGrpcPath = "/match.MatchService/NotifyChallengeInvite";
using AsyncMatchServiceNotifyChallengeInviteHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncMatchServiceNotifyChallengeInviteHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceNotifyChallengeInviteFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::ChallengeInviteS2C&)>;
extern AsyncMatchServiceNotifyChallengeInviteFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteFailedHandler;

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request);
void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceNotifyChallengeResult
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceNotifyChallengeResultGrpcPath = "/match.MatchService/NotifyChallengeResult";
using AsyncMatchServiceNotifyChallengeResultHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncMatchServiceNotifyChallengeResultHandlerFunctionType AsyncMatchServiceNotifyChallengeResultHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceNotifyChallengeResultFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::ChallengeResultS2C&)>;
extern AsyncMatchServiceNotifyChallengeResultFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeResultFailedHandler;

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request);
void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceWatchBattle
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceWatchBattleGrpcPath = "/match.MatchService/WatchBattle";
using AsyncMatchServiceWatchBattleHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::WatchBattleResponse&)>;
extern AsyncMatchServiceWatchBattleHandlerFunctionType AsyncMatchServiceWatchBattleHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceWatchBattleFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::WatchBattleRequest&)>;
extern AsyncMatchServiceWatchBattleFailedHandlerFunctionType AsyncMatchServiceWatchBattleFailedHandler;

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request);
void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request, GrpcDone<::match::WatchBattleResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceRequestBattleTicket
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceRequestBattleTicketGrpcPath = "/match.MatchService/RequestBattleTicket";
using AsyncMatchServiceRequestBattleTicketHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::RequestBattleTicketResponse&)>;
extern AsyncMatchServiceRequestBattleTicketHandlerFunctionType AsyncMatchServiceRequestBattleTicketHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceRequestBattleTicketFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::RequestBattleTicketRequest&)>;
extern AsyncMatchServiceRequestBattleTicketFailedHandlerFunctionType AsyncMatchServiceRequestBattleTicketFailedHandler;

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request);
void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request, GrpcDone<::RequestBattleTicketResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region MatchServiceListWatchableBattles
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchServiceListWatchableBattlesGrpcPath = "/match.MatchService/ListWatchableBattles";
using AsyncMatchServiceListWatchableBattlesHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::match::ListWatchableBattlesResponse&)>;
extern AsyncMatchServiceListWatchableBattlesHandlerFunctionType AsyncMatchServiceListWatchableBattlesHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncMatchServiceListWatchableBattlesFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::match::ListWatchableBattlesRequest&)>;
extern AsyncMatchServiceListWatchableBattlesFailedHandlerFunctionType AsyncMatchServiceListWatchableBattlesFailedHandler;

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request);
void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request, GrpcDone<::match::ListWatchableBattlesResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetMatchServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMatchServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetMatchServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetMatchServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetMatchServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace match
