#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/player_locator_service_metadata.h"

namespace playerlocator {
#pragma region PlayerLocatorSetLocation
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetLocationGrpcPath = "/playerlocator.PlayerLocator/SetLocation";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorSetLocationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorGetLocation
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorGetLocationGrpcPath = "/playerlocator.PlayerLocator/GetLocation";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, GrpcDone<::playerlocator::PlayerLocation> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorGetLocationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorMarkOffline
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorMarkOfflineGrpcPath = "/playerlocator.PlayerLocator/MarkOffline";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorMarkOfflineErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorSetSession
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetSessionGrpcPath = "/playerlocator.PlayerLocator/SetSession";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorSetSessionErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorGetSession
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorGetSessionGrpcPath = "/playerlocator.PlayerLocator/GetSession";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request, GrpcDone<::playerlocator::GetSessionResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorGetSessionErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorSetDisconnecting
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorSetDisconnectingGrpcPath = "/playerlocator.PlayerLocator/SetDisconnecting";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorSetDisconnectingErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region PlayerLocatorReconnect
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kPlayerLocatorReconnectGrpcPath = "/playerlocator.PlayerLocator/Reconnect";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request, GrpcDone<::playerlocator::ReconnectResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendPlayerLocatorReconnectErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetPlayerLocatorCallDeadline(std::chrono::milliseconds deadline);

}// namespace playerlocator
