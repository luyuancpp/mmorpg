#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/player_locator_service_metadata.h"

namespace playerlocator {
#pragma region PlayerLocatorSetLocation
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetLocationGrpcPath = "/playerlocator.PlayerLocator/SetLocation";
using AsyncPlayerLocatorSetLocationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncPlayerLocatorSetLocationHandlerFunctionType AsyncPlayerLocatorSetLocationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorSetLocationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::PlayerLocation&)>;
extern AsyncPlayerLocatorSetLocationFailedHandlerFunctionType AsyncPlayerLocatorSetLocationFailedHandler;

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request);
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorGetLocation
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorGetLocationGrpcPath = "/playerlocator.PlayerLocator/GetLocation";
using AsyncPlayerLocatorGetLocationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::playerlocator::PlayerLocation&)>;
extern AsyncPlayerLocatorGetLocationHandlerFunctionType AsyncPlayerLocatorGetLocationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorGetLocationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::PlayerId&)>;
extern AsyncPlayerLocatorGetLocationFailedHandlerFunctionType AsyncPlayerLocatorGetLocationFailedHandler;

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request);
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, GrpcDone<::playerlocator::PlayerLocation> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorMarkOffline
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorMarkOfflineGrpcPath = "/playerlocator.PlayerLocator/MarkOffline";
using AsyncPlayerLocatorMarkOfflineHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncPlayerLocatorMarkOfflineHandlerFunctionType AsyncPlayerLocatorMarkOfflineHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorMarkOfflineFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::MarkOfflineRequest&)>;
extern AsyncPlayerLocatorMarkOfflineFailedHandlerFunctionType AsyncPlayerLocatorMarkOfflineFailedHandler;

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request);
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorSetSession
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetSessionGrpcPath = "/playerlocator.PlayerLocator/SetSession";
using AsyncPlayerLocatorSetSessionHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncPlayerLocatorSetSessionHandlerFunctionType AsyncPlayerLocatorSetSessionHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorSetSessionFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::SetSessionRequest&)>;
extern AsyncPlayerLocatorSetSessionFailedHandlerFunctionType AsyncPlayerLocatorSetSessionFailedHandler;

void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request);
void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorGetSession
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorGetSessionGrpcPath = "/playerlocator.PlayerLocator/GetSession";
using AsyncPlayerLocatorGetSessionHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::playerlocator::GetSessionResponse&)>;
extern AsyncPlayerLocatorGetSessionHandlerFunctionType AsyncPlayerLocatorGetSessionHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorGetSessionFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::GetSessionRequest&)>;
extern AsyncPlayerLocatorGetSessionFailedHandlerFunctionType AsyncPlayerLocatorGetSessionFailedHandler;

void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request);
void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request, GrpcDone<::playerlocator::GetSessionResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorSetDisconnecting
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetDisconnectingGrpcPath = "/playerlocator.PlayerLocator/SetDisconnecting";
using AsyncPlayerLocatorSetDisconnectingHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncPlayerLocatorSetDisconnectingHandlerFunctionType AsyncPlayerLocatorSetDisconnectingHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorSetDisconnectingFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::SetDisconnectingRequest&)>;
extern AsyncPlayerLocatorSetDisconnectingFailedHandlerFunctionType AsyncPlayerLocatorSetDisconnectingFailedHandler;

void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request);
void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region PlayerLocatorReconnect
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorReconnectGrpcPath = "/playerlocator.PlayerLocator/Reconnect";
using AsyncPlayerLocatorReconnectHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::playerlocator::ReconnectResponse&)>;
extern AsyncPlayerLocatorReconnectHandlerFunctionType AsyncPlayerLocatorReconnectHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncPlayerLocatorReconnectFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::playerlocator::ReconnectRequest&)>;
extern AsyncPlayerLocatorReconnectFailedHandlerFunctionType AsyncPlayerLocatorReconnectFailedHandler;

void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request);
void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request, GrpcDone<::playerlocator::ReconnectResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetPlayerLocatorHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerLocatorIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerLocatorFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetPlayerLocatorIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetPlayerLocatorCallDeadline(std::chrono::milliseconds deadline);

}// namespace playerlocator
