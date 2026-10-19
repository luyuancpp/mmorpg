#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_call_types.h"

#include "rpc/service_metadata/scene_manager_service_service_metadata.h"

namespace scene_manager {
#pragma region SceneManagerCreateScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerCreateSceneGrpcPath = "/scene_manager.SceneManager/CreateScene";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, GrpcDone<::scene_manager::CreateSceneResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendSceneManagerCreateSceneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region SceneManagerDestroyScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerDestroySceneGrpcPath = "/scene_manager.SceneManager/DestroyScene";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendSceneManagerDestroySceneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region SceneManagerEnterScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerEnterSceneGrpcPath = "/scene_manager.SceneManager/EnterScene";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request, GrpcDone<::scene_manager::EnterSceneResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendSceneManagerEnterSceneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region SceneManagerLeaveScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerLeaveSceneGrpcPath = "/scene_manager.SceneManager/LeaveScene";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendSceneManagerLeaveSceneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace scene_manager
