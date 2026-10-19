#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_call_types.h"

#include "rpc/service_metadata/data_service_service_metadata.h"

namespace data_service {
#pragma region DataServiceLoadPlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceLoadPlayerDataGrpcPath = "/data_service.DataService/LoadPlayerData";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, GrpcDone<::data_service::LoadPlayerDataResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceLoadPlayerDataErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceSavePlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceSavePlayerDataGrpcPath = "/data_service.DataService/SavePlayerData";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, GrpcDone<::data_service::SavePlayerDataResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceSavePlayerDataErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceGetPlayerField
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerFieldGrpcPath = "/data_service.DataService/GetPlayerField";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, GrpcDone<::data_service::GetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceGetPlayerFieldErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceSetPlayerField
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceSetPlayerFieldGrpcPath = "/data_service.DataService/SetPlayerField";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, GrpcDone<::data_service::SetPlayerFieldResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceSetPlayerFieldErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceRegisterPlayerZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRegisterPlayerZoneGrpcPath = "/data_service.DataService/RegisterPlayerZone";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceRegisterPlayerZoneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerHomeZoneGrpcPath = "/data_service.DataService/GetPlayerHomeZone";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, GrpcDone<::data_service::GetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceGetPlayerHomeZoneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchGetPlayerHomeZoneGrpcPath = "/data_service.DataService/BatchGetPlayerHomeZone";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceBatchGetPlayerHomeZoneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceRemapHomeZoneForMerge
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRemapHomeZoneForMergeGrpcPath = "/data_service.DataService/RemapHomeZoneForMerge";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceRemapHomeZoneForMergeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceDeletePlayerData
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceDeletePlayerDataGrpcPath = "/data_service.DataService/DeletePlayerData";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, GrpcDone<::data_service::DeletePlayerDataResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceDeletePlayerDataErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceCreatePlayerSnapshot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceCreatePlayerSnapshotGrpcPath = "/data_service.DataService/CreatePlayerSnapshot";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, GrpcDone<::data_service::CreatePlayerSnapshotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceCreatePlayerSnapshotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceListPlayerSnapshots
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceListPlayerSnapshotsGrpcPath = "/data_service.DataService/ListPlayerSnapshots";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, GrpcDone<::data_service::ListPlayerSnapshotsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceListPlayerSnapshotsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceGetPlayerSnapshotDiff
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerSnapshotDiffGrpcPath = "/data_service.DataService/GetPlayerSnapshotDiff";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceGetPlayerSnapshotDiffErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceRollbackPlayer
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackPlayerGrpcPath = "/data_service.DataService/RollbackPlayer";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, GrpcDone<::data_service::RollbackPlayerResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceRollbackPlayerErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceRollbackZone
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackZoneGrpcPath = "/data_service.DataService/RollbackZone";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, GrpcDone<::data_service::RollbackZoneResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceRollbackZoneErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceRollbackAll
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceRollbackAllGrpcPath = "/data_service.DataService/RollbackAll";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, GrpcDone<::data_service::RollbackAllResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceRollbackAllErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceBatchRecallItems
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchRecallItemsGrpcPath = "/data_service.DataService/BatchRecallItems";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, GrpcDone<::data_service::BatchRecallItemsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceBatchRecallItemsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceQueryTransactionLog
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceQueryTransactionLogGrpcPath = "/data_service.DataService/QueryTransactionLog";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, GrpcDone<::data_service::QueryTransactionLogResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceQueryTransactionLogErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceCreateEventSnapshot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceCreateEventSnapshotGrpcPath = "/data_service.DataService/CreateEventSnapshot";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, GrpcDone<::data_service::CreateEventSnapshotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceCreateEventSnapshotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceAllocateIdSegment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceAllocateIdSegmentGrpcPath = "/data_service.DataService/AllocateIdSegment";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, GrpcDone<::data_service::AllocateIdSegmentResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceAllocateIdSegmentErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceReservePlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceReservePlayerNameGrpcPath = "/data_service.DataService/ReservePlayerName";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, GrpcDone<::data_service::ReservePlayerNameResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceReservePlayerNameErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceReleasePlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceReleasePlayerNameGrpcPath = "/data_service.DataService/ReleasePlayerName";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceReleasePlayerNameErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceBatchGetPlayerName
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceBatchGetPlayerNameGrpcPath = "/data_service.DataService/BatchGetPlayerName";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, GrpcDone<::data_service::BatchGetPlayerNameResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceBatchGetPlayerNameErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceGetPlayerAssetOpLedger
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerAssetOpLedgerGrpcPath = "/data_service.DataService/GetPlayerAssetOpLedger";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceGetPlayerAssetOpLedgerErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region DataServiceGetPlayerInboxLedger
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kDataServiceGetPlayerInboxLedgerGrpcPath = "/data_service.DataService/GetPlayerInboxLedger";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request, GrpcDone<::data_service::GetPlayerInboxLedgerResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendDataServiceGetPlayerInboxLedgerErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetDataServiceCallDeadline(std::chrono::milliseconds deadline);

}// namespace data_service
