#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/data_service_service_metadata.h"

namespace data_service {
#pragma region DataServiceLoadPlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceLoadPlayerDataGrpcPath = "/data_service.DataService/LoadPlayerData";
using AsyncDataServiceLoadPlayerDataHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::LoadPlayerDataResponse&)>;
extern AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::LoadPlayerDataRequest&)>;
extern AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType AsyncDataServiceLoadPlayerDataFailedHandler;

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, GrpcDone<::data_service::LoadPlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceSavePlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceSavePlayerDataGrpcPath = "/data_service.DataService/SavePlayerData";
using AsyncDataServiceSavePlayerDataHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::SavePlayerDataResponse&)>;
extern AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceSavePlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::SavePlayerDataRequest&)>;
extern AsyncDataServiceSavePlayerDataFailedHandlerFunctionType AsyncDataServiceSavePlayerDataFailedHandler;

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, GrpcDone<::data_service::SavePlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerField
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerFieldGrpcPath = "/data_service.DataService/GetPlayerField";
using AsyncDataServiceGetPlayerFieldHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::GetPlayerFieldResponse&)>;
extern AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerFieldRequest&)>;
extern AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType AsyncDataServiceGetPlayerFieldFailedHandler;

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, GrpcDone<::data_service::GetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceSetPlayerField
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceSetPlayerFieldGrpcPath = "/data_service.DataService/SetPlayerField";
using AsyncDataServiceSetPlayerFieldHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::SetPlayerFieldResponse&)>;
extern AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::SetPlayerFieldRequest&)>;
extern AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType AsyncDataServiceSetPlayerFieldFailedHandler;

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, GrpcDone<::data_service::SetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRegisterPlayerZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRegisterPlayerZoneGrpcPath = "/data_service.DataService/RegisterPlayerZone";
using AsyncDataServiceRegisterPlayerZoneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::google::protobuf::Empty&)>;
extern AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RegisterPlayerZoneRequest&)>;
extern AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType AsyncDataServiceRegisterPlayerZoneFailedHandler;

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerHomeZoneGrpcPath = "/data_service.DataService/GetPlayerHomeZone";
using AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::GetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerHomeZoneRequest&)>;
extern AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneFailedHandler;

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, GrpcDone<::data_service::GetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchGetPlayerHomeZoneGrpcPath = "/data_service.DataService/BatchGetPlayerHomeZone";
using AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::BatchGetPlayerHomeZoneResponse&)>;
extern AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchGetPlayerHomeZoneRequest&)>;
extern AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler;

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRemapHomeZoneForMerge
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRemapHomeZoneForMergeGrpcPath = "/data_service.DataService/RemapHomeZoneForMerge";
using AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::RemapHomeZoneForMergeResponse&)>;
extern AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RemapHomeZoneForMergeRequest&)>;
extern AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeFailedHandler;

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request);
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceDeletePlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceDeletePlayerDataGrpcPath = "/data_service.DataService/DeletePlayerData";
using AsyncDataServiceDeletePlayerDataHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::DeletePlayerDataResponse&)>;
extern AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::DeletePlayerDataRequest&)>;
extern AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType AsyncDataServiceDeletePlayerDataFailedHandler;

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, GrpcDone<::data_service::DeletePlayerDataResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceCreatePlayerSnapshot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceCreatePlayerSnapshotGrpcPath = "/data_service.DataService/CreatePlayerSnapshot";
using AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::CreatePlayerSnapshotResponse&)>;
extern AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::CreatePlayerSnapshotRequest&)>;
extern AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotFailedHandler;

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request);
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, GrpcDone<::data_service::CreatePlayerSnapshotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceListPlayerSnapshots
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceListPlayerSnapshotsGrpcPath = "/data_service.DataService/ListPlayerSnapshots";
using AsyncDataServiceListPlayerSnapshotsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::ListPlayerSnapshotsResponse&)>;
extern AsyncDataServiceListPlayerSnapshotsHandlerFunctionType AsyncDataServiceListPlayerSnapshotsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ListPlayerSnapshotsRequest&)>;
extern AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType AsyncDataServiceListPlayerSnapshotsFailedHandler;

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request);
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, GrpcDone<::data_service::ListPlayerSnapshotsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerSnapshotDiff
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerSnapshotDiffGrpcPath = "/data_service.DataService/GetPlayerSnapshotDiff";
using AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::GetPlayerSnapshotDiffResponse&)>;
extern AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerSnapshotDiffRequest&)>;
extern AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffFailedHandler;

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request);
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackPlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackPlayerGrpcPath = "/data_service.DataService/RollbackPlayer";
using AsyncDataServiceRollbackPlayerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::RollbackPlayerResponse&)>;
extern AsyncDataServiceRollbackPlayerHandlerFunctionType AsyncDataServiceRollbackPlayerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackPlayerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackPlayerRequest&)>;
extern AsyncDataServiceRollbackPlayerFailedHandlerFunctionType AsyncDataServiceRollbackPlayerFailedHandler;

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request);
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, GrpcDone<::data_service::RollbackPlayerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackZoneGrpcPath = "/data_service.DataService/RollbackZone";
using AsyncDataServiceRollbackZoneHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::RollbackZoneResponse&)>;
extern AsyncDataServiceRollbackZoneHandlerFunctionType AsyncDataServiceRollbackZoneHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackZoneFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackZoneRequest&)>;
extern AsyncDataServiceRollbackZoneFailedHandlerFunctionType AsyncDataServiceRollbackZoneFailedHandler;

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request);
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, GrpcDone<::data_service::RollbackZoneResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceRollbackAll
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackAllGrpcPath = "/data_service.DataService/RollbackAll";
using AsyncDataServiceRollbackAllHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::RollbackAllResponse&)>;
extern AsyncDataServiceRollbackAllHandlerFunctionType AsyncDataServiceRollbackAllHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceRollbackAllFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::RollbackAllRequest&)>;
extern AsyncDataServiceRollbackAllFailedHandlerFunctionType AsyncDataServiceRollbackAllFailedHandler;

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request);
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, GrpcDone<::data_service::RollbackAllResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchRecallItems
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchRecallItemsGrpcPath = "/data_service.DataService/BatchRecallItems";
using AsyncDataServiceBatchRecallItemsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::BatchRecallItemsResponse&)>;
extern AsyncDataServiceBatchRecallItemsHandlerFunctionType AsyncDataServiceBatchRecallItemsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchRecallItemsRequest&)>;
extern AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType AsyncDataServiceBatchRecallItemsFailedHandler;

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request);
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, GrpcDone<::data_service::BatchRecallItemsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceQueryTransactionLog
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceQueryTransactionLogGrpcPath = "/data_service.DataService/QueryTransactionLog";
using AsyncDataServiceQueryTransactionLogHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::QueryTransactionLogResponse&)>;
extern AsyncDataServiceQueryTransactionLogHandlerFunctionType AsyncDataServiceQueryTransactionLogHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::QueryTransactionLogRequest&)>;
extern AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType AsyncDataServiceQueryTransactionLogFailedHandler;

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request);
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, GrpcDone<::data_service::QueryTransactionLogResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceCreateEventSnapshot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceCreateEventSnapshotGrpcPath = "/data_service.DataService/CreateEventSnapshot";
using AsyncDataServiceCreateEventSnapshotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::CreateEventSnapshotResponse&)>;
extern AsyncDataServiceCreateEventSnapshotHandlerFunctionType AsyncDataServiceCreateEventSnapshotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::CreateEventSnapshotRequest&)>;
extern AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType AsyncDataServiceCreateEventSnapshotFailedHandler;

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request);
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, GrpcDone<::data_service::CreateEventSnapshotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceAllocateIdSegment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceAllocateIdSegmentGrpcPath = "/data_service.DataService/AllocateIdSegment";
using AsyncDataServiceAllocateIdSegmentHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::AllocateIdSegmentResponse&)>;
extern AsyncDataServiceAllocateIdSegmentHandlerFunctionType AsyncDataServiceAllocateIdSegmentHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::AllocateIdSegmentRequest&)>;
extern AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType AsyncDataServiceAllocateIdSegmentFailedHandler;

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request);
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, GrpcDone<::data_service::AllocateIdSegmentResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceReservePlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceReservePlayerNameGrpcPath = "/data_service.DataService/ReservePlayerName";
using AsyncDataServiceReservePlayerNameHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::ReservePlayerNameResponse&)>;
extern AsyncDataServiceReservePlayerNameHandlerFunctionType AsyncDataServiceReservePlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceReservePlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ReservePlayerNameRequest&)>;
extern AsyncDataServiceReservePlayerNameFailedHandlerFunctionType AsyncDataServiceReservePlayerNameFailedHandler;

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request);
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, GrpcDone<::data_service::ReservePlayerNameResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceReleasePlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceReleasePlayerNameGrpcPath = "/data_service.DataService/ReleasePlayerName";
using AsyncDataServiceReleasePlayerNameHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::google::protobuf::Empty&)>;
extern AsyncDataServiceReleasePlayerNameHandlerFunctionType AsyncDataServiceReleasePlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::ReleasePlayerNameRequest&)>;
extern AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType AsyncDataServiceReleasePlayerNameFailedHandler;

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request);
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceBatchGetPlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchGetPlayerNameGrpcPath = "/data_service.DataService/BatchGetPlayerName";
using AsyncDataServiceBatchGetPlayerNameHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::BatchGetPlayerNameResponse&)>;
extern AsyncDataServiceBatchGetPlayerNameHandlerFunctionType AsyncDataServiceBatchGetPlayerNameHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::BatchGetPlayerNameRequest&)>;
extern AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerNameFailedHandler;

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request);
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, GrpcDone<::data_service::BatchGetPlayerNameResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerAssetOpLedger
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerAssetOpLedgerGrpcPath = "/data_service.DataService/GetPlayerAssetOpLedger";
using AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::GetPlayerAssetOpLedgerResponse&)>;
extern AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerAssetOpLedgerRequest&)>;
extern AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler;

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request);
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region DataServiceGetPlayerInboxLedger
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerInboxLedgerGrpcPath = "/data_service.DataService/GetPlayerInboxLedger";
using AsyncDataServiceGetPlayerInboxLedgerHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::data_service::GetPlayerInboxLedgerResponse&)>;
extern AsyncDataServiceGetPlayerInboxLedgerHandlerFunctionType AsyncDataServiceGetPlayerInboxLedgerHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncDataServiceGetPlayerInboxLedgerFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::data_service::GetPlayerInboxLedgerRequest&)>;
extern AsyncDataServiceGetPlayerInboxLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerInboxLedgerFailedHandler;

void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request);
void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request, GrpcDone<::data_service::GetPlayerInboxLedgerResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetDataServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDataServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDataServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetDataServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetDataServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace data_service
