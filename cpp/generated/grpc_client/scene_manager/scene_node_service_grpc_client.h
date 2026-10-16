#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <deque>
#include "core/network/grpc_client_runtime.h"
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"
#include "proto/scene_manager/scene_node_service.grpc.pb.h"

#include "rpc/service_metadata/scene_node_service_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace scene_node {
using SceneNodeGrpcStubPtr = std::unique_ptr<SceneNodeGrpc::Stub>;
#pragma region SceneNodeGrpcCreateScene

struct AsyncSceneNodeGrpcCreateSceneGrpcClient {
    uint32_t messageId{ SceneNodeGrpcCreateSceneMessageId };
    ClientContext context;
    Status status;
    ::CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::CreateSceneResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::CreateSceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcCreateSceneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::CreateSceneResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcCreateSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::CreateSceneResponse&)>;
extern AsyncSceneNodeGrpcCreateSceneHandlerFunctionType AsyncSceneNodeGrpcCreateSceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcCreateSceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::CreateSceneRequest&)>;
extern AsyncSceneNodeGrpcCreateSceneFailedHandlerFunctionType AsyncSceneNodeGrpcCreateSceneFailedHandler;

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request);
void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, GrpcDone<::CreateSceneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcDestroyScene

struct AsyncSceneNodeGrpcDestroySceneGrpcClient {
    uint32_t messageId{ SceneNodeGrpcDestroySceneMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::DestroySceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcDestroySceneHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcDestroySceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncSceneNodeGrpcDestroySceneHandlerFunctionType AsyncSceneNodeGrpcDestroySceneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcDestroySceneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::DestroySceneRequest&)>;
extern AsyncSceneNodeGrpcDestroySceneFailedHandlerFunctionType AsyncSceneNodeGrpcDestroySceneFailedHandler;

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request);
void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcReleasePlayer

struct AsyncSceneNodeGrpcReleasePlayerGrpcClient {
    uint32_t messageId{ SceneNodeGrpcReleasePlayerMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::scene_node::ReleasePlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcReleasePlayerHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcReleasePlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::scene_node::ReleasePlayerRequest&)>;
extern AsyncSceneNodeGrpcReleasePlayerFailedHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerFailedHandler;

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request);
void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcPrepareBattle

struct AsyncSceneNodeGrpcPrepareBattleGrpcClient {
    uint32_t messageId{ SceneNodeGrpcPrepareBattleMessageId };
    ClientContext context;
    Status status;
    ::PrepareBattleResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::PrepareBattleResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::PrepareBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcPrepareBattleHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::PrepareBattleResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::PrepareBattleResponse&)>;
extern AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcPrepareBattleFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::PrepareBattleRequest&)>;
extern AsyncSceneNodeGrpcPrepareBattleFailedHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleFailedHandler;

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request);
void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request, GrpcDone<::PrepareBattleResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcCancelBattlePrepare

struct AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient {
    uint32_t messageId{ SceneNodeGrpcCancelBattlePrepareMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::CancelBattlePrepareRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcCancelBattlePrepareHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::Empty> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcCancelBattlePrepareFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::CancelBattlePrepareRequest&)>;
extern AsyncSceneNodeGrpcCancelBattlePrepareFailedHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler;

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request);
void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcAssetDebit

struct AsyncSceneNodeGrpcAssetDebitGrpcClient {
    uint32_t messageId{ SceneNodeGrpcAssetDebitMessageId };
    ClientContext context;
    Status status;
    ::AssetOpResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::AssetOpResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetDebitHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::AssetOpResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcAssetDebitHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
extern AsyncSceneNodeGrpcAssetDebitHandlerFunctionType AsyncSceneNodeGrpcAssetDebitHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcAssetDebitFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::AssetOpRequest&)>;
extern AsyncSceneNodeGrpcAssetDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetDebitFailedHandler;

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request);
void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcAssetAbortDebit

struct AsyncSceneNodeGrpcAssetAbortDebitGrpcClient {
    uint32_t messageId{ SceneNodeGrpcAssetAbortDebitMessageId };
    ClientContext context;
    Status status;
    ::AssetOpResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::AssetOpResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetAbortDebitHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::AssetOpResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
extern AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcAssetAbortDebitFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::AssetOpRequest&)>;
extern AsyncSceneNodeGrpcAssetAbortDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitFailedHandler;

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request);
void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region SceneNodeGrpcAssetCredit

struct AsyncSceneNodeGrpcAssetCreditGrpcClient {
    uint32_t messageId{ SceneNodeGrpcAssetCreditMessageId };
    ClientContext context;
    Status status;
    ::AssetOpResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::AssetOpResponse>> response_reader;
    // 失败时交还失败处理器:应答里的回显字段与服务端回写的 metadata 这时都拿不到(见 GrpcCallFailure)。
    // 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetCreditHandler / FailedHandler。
    // 调用对象在对象池里不移动,回调捕获的上下文随它活到完成(不放进 ECS 组件)。
    GrpcDone<::AssetOpResponse> done;
    GrpcClientRuntimePtr runtime;
};

using AsyncSceneNodeGrpcAssetCreditHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
extern AsyncSceneNodeGrpcAssetCreditHandlerFunctionType AsyncSceneNodeGrpcAssetCreditHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcAssetCreditFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::AssetOpRequest&)>;
extern AsyncSceneNodeGrpcAssetCreditFailedHandlerFunctionType AsyncSceneNodeGrpcAssetCreditFailedHandler;

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request);
void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetSceneNodeServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneNodeServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetSceneNodeServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。流式调用不设。
void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline);
void HandleSceneNodeServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag);
void InitSceneNodeServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace scene_node
