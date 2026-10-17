#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/scene_manager_service_service_metadata.h"

namespace scene_manager {
#pragma region SceneManagerCreateScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerCreateSceneGrpcPath = "/scene_manager.SceneManager/CreateScene";
using AsyncSceneManagerCreateSceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::scene_manager::CreateSceneResponse&)>;
extern AsyncSceneManagerCreateSceneHandlerFunctionType AsyncSceneManagerCreateSceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneManagerCreateSceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::scene_manager::CreateSceneRequest&)>;
extern AsyncSceneManagerCreateSceneFailedHandlerFunctionType AsyncSceneManagerCreateSceneFailedHandler;

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request);
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, GrpcDone<::scene_manager::CreateSceneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneManagerDestroyScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerDestroySceneGrpcPath = "/scene_manager.SceneManager/DestroyScene";
using AsyncSceneManagerDestroySceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncSceneManagerDestroySceneHandlerFunctionType AsyncSceneManagerDestroySceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneManagerDestroySceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::scene_manager::DestroySceneRequest&)>;
extern AsyncSceneManagerDestroySceneFailedHandlerFunctionType AsyncSceneManagerDestroySceneFailedHandler;

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request);
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneManagerEnterScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerEnterSceneGrpcPath = "/scene_manager.SceneManager/EnterScene";
using AsyncSceneManagerEnterSceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::scene_manager::EnterSceneResponse&)>;
extern AsyncSceneManagerEnterSceneHandlerFunctionType AsyncSceneManagerEnterSceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneManagerEnterSceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::scene_manager::EnterSceneRequest&)>;
extern AsyncSceneManagerEnterSceneFailedHandlerFunctionType AsyncSceneManagerEnterSceneFailedHandler;

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request);
void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request, GrpcDone<::scene_manager::EnterSceneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneManagerLeaveScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneManagerLeaveSceneGrpcPath = "/scene_manager.SceneManager/LeaveScene";
using AsyncSceneManagerLeaveSceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncSceneManagerLeaveSceneHandlerFunctionType AsyncSceneManagerLeaveSceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneManagerLeaveSceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::scene_manager::LeaveSceneRequest&)>;
extern AsyncSceneManagerLeaveSceneFailedHandlerFunctionType AsyncSceneManagerLeaveSceneFailedHandler;

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request);
void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetSceneManagerServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneManagerServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetSceneManagerServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace scene_manager
