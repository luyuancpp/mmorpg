#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/login_service_metadata.h"

namespace loginpb {
#pragma region ClientPlayerLoginLogin
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginLoginGrpcPath = "/loginpb.ClientPlayerLogin/Login";
using AsyncClientPlayerLoginLoginHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::LoginResponse&)>;
extern AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginLoginFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::LoginRequest&)>;
extern AsyncClientPlayerLoginLoginFailedHandlerFunctionType AsyncClientPlayerLoginLoginFailedHandler;

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, GrpcDone<::loginpb::LoginResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerLoginCreatePlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginCreatePlayerGrpcPath = "/loginpb.ClientPlayerLogin/CreatePlayer";
using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::CreatePlayerResponse&)>;
extern AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginCreatePlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::CreatePlayerRequest&)>;
extern AsyncClientPlayerLoginCreatePlayerFailedHandlerFunctionType AsyncClientPlayerLoginCreatePlayerFailedHandler;

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, GrpcDone<::loginpb::CreatePlayerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerLoginEnterGame
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginEnterGameGrpcPath = "/loginpb.ClientPlayerLogin/EnterGame";
using AsyncClientPlayerLoginEnterGameHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::EnterGameResponse&)>;
extern AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginEnterGameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::EnterGameRequest&)>;
extern AsyncClientPlayerLoginEnterGameFailedHandlerFunctionType AsyncClientPlayerLoginEnterGameFailedHandler;

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, GrpcDone<::loginpb::EnterGameResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerLoginLeaveGame
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginLeaveGameGrpcPath = "/loginpb.ClientPlayerLogin/LeaveGame";
using AsyncClientPlayerLoginLeaveGameHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::LoginEmptyResponse&)>;
extern AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginLeaveGameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::LeaveGameRequest&)>;
extern AsyncClientPlayerLoginLeaveGameFailedHandlerFunctionType AsyncClientPlayerLoginLeaveGameFailedHandler;

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, GrpcDone<::loginpb::LoginEmptyResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerLoginDisconnect
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginDisconnectGrpcPath = "/loginpb.ClientPlayerLogin/Disconnect";
using AsyncClientPlayerLoginDisconnectHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::LoginEmptyResponse&)>;
extern AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginDisconnectFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::LoginNodeDisconnectRequest&)>;
extern AsyncClientPlayerLoginDisconnectFailedHandlerFunctionType AsyncClientPlayerLoginDisconnectFailedHandler;

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, GrpcDone<::loginpb::LoginEmptyResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerLoginRefreshToken
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerLoginRefreshTokenGrpcPath = "/loginpb.ClientPlayerLogin/RefreshToken";
using AsyncClientPlayerLoginRefreshTokenHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::RefreshTokenResponse&)>;
extern AsyncClientPlayerLoginRefreshTokenHandlerFunctionType AsyncClientPlayerLoginRefreshTokenHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerLoginRefreshTokenFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::RefreshTokenRequest&)>;
extern AsyncClientPlayerLoginRefreshTokenFailedHandlerFunctionType AsyncClientPlayerLoginRefreshTokenFailedHandler;

void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request);
void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request, GrpcDone<::loginpb::RefreshTokenResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LoginPreGateAssignGate
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLoginPreGateAssignGateGrpcPath = "/loginpb.LoginPreGate/AssignGate";
using AsyncLoginPreGateAssignGateHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::AssignGateResponse&)>;
extern AsyncLoginPreGateAssignGateHandlerFunctionType AsyncLoginPreGateAssignGateHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLoginPreGateAssignGateFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::AssignGateRequest&)>;
extern AsyncLoginPreGateAssignGateFailedHandlerFunctionType AsyncLoginPreGateAssignGateFailedHandler;

void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request);
void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request, GrpcDone<::loginpb::AssignGateResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LoginPreGateQueryQueueStatus
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLoginPreGateQueryQueueStatusGrpcPath = "/loginpb.LoginPreGate/QueryQueueStatus";
using AsyncLoginPreGateQueryQueueStatusHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::QueryQueueStatusResponse&)>;
extern AsyncLoginPreGateQueryQueueStatusHandlerFunctionType AsyncLoginPreGateQueryQueueStatusHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLoginPreGateQueryQueueStatusFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::QueryQueueStatusRequest&)>;
extern AsyncLoginPreGateQueryQueueStatusFailedHandlerFunctionType AsyncLoginPreGateQueryQueueStatusFailedHandler;

void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request);
void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request, GrpcDone<::loginpb::QueryQueueStatusResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region LoginAdminRemovePlayersFromAccounts
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kLoginAdminRemovePlayersFromAccountsGrpcPath = "/loginpb.LoginAdmin/RemovePlayersFromAccounts";
using AsyncLoginAdminRemovePlayersFromAccountsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::loginpb::RemovePlayersFromAccountsResponse&)>;
extern AsyncLoginAdminRemovePlayersFromAccountsHandlerFunctionType AsyncLoginAdminRemovePlayersFromAccountsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncLoginAdminRemovePlayersFromAccountsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::loginpb::RemovePlayersFromAccountsRequest&)>;
extern AsyncLoginAdminRemovePlayersFromAccountsFailedHandlerFunctionType AsyncLoginAdminRemovePlayersFromAccountsFailedHandler;

void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request);
void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request, GrpcDone<::loginpb::RemovePlayersFromAccountsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetLoginHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetLoginIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetLoginFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetLoginIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetLoginCallDeadline(std::chrono::milliseconds deadline);

}// namespace loginpb
