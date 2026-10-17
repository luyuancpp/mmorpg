#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/scene_node_service_service_metadata.h"

namespace scene_node {
#pragma region SceneNodeGrpcCreateScene
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcCreateSceneGrpcPath = "/scene_node.SceneNodeGrpc/CreateScene";
using AsyncSceneNodeGrpcCreateSceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::CreateSceneResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcDestroySceneGrpcPath = "/scene_node.SceneNodeGrpc/DestroyScene";
using AsyncSceneNodeGrpcDestroySceneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcReleasePlayerGrpcPath = "/scene_node.SceneNodeGrpc/ReleasePlayer";
using AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcPrepareBattleGrpcPath = "/scene_node.SceneNodeGrpc/PrepareBattle";
using AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::PrepareBattleResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcCancelBattlePrepareGrpcPath = "/scene_node.SceneNodeGrpc/CancelBattlePrepare";
using AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcAssetDebitGrpcPath = "/scene_node.SceneNodeGrpc/AssetDebit";
using AsyncSceneNodeGrpcAssetDebitHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::AssetOpResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcAssetAbortDebitGrpcPath = "/scene_node.SceneNodeGrpc/AssetAbortDebit";
using AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::AssetOpResponse&)>;
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
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcAssetCreditGrpcPath = "/scene_node.SceneNodeGrpc/AssetCredit";
using AsyncSceneNodeGrpcAssetCreditHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::AssetOpResponse&)>;
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
#pragma region SceneNodeGrpcAssetAckReceipt
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kSceneNodeGrpcAssetAckReceiptGrpcPath = "/scene_node.SceneNodeGrpc/AssetAckReceipt";
using AsyncSceneNodeGrpcAssetAckReceiptHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::AssetOpResponse&)>;
extern AsyncSceneNodeGrpcAssetAckReceiptHandlerFunctionType AsyncSceneNodeGrpcAssetAckReceiptHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncSceneNodeGrpcAssetAckReceiptFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::AssetOpRequest&)>;
extern AsyncSceneNodeGrpcAssetAckReceiptFailedHandlerFunctionType AsyncSceneNodeGrpcAssetAckReceiptFailedHandler;

void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request);
void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetSceneNodeServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneNodeServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetSceneNodeServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace scene_node
