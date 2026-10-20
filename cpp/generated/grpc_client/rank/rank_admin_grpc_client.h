#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_client_call.h"

#include "rpc/service_metadata/rank_admin_service_metadata.h"

namespace rankpb {
#pragma region RankAdminOpenRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminOpenRankGenerationGrpcPath = "/rankpb.RankAdmin/OpenRankGeneration";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request, GrpcDone<::rankpb::OpenRankGenerationResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendRankAdminOpenRankGenerationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region RankAdminSubmitRankScores
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminSubmitRankScoresGrpcPath = "/rankpb.RankAdmin/SubmitRankScores";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request, GrpcDone<::rankpb::SubmitRankScoresResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendRankAdminSubmitRankScoresErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region RankAdminFreezeRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminFreezeRankGenerationGrpcPath = "/rankpb.RankAdmin/FreezeRankGeneration";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request, GrpcDone<::rankpb::FreezeRankGenerationResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendRankAdminFreezeRankGenerationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region RankAdminRemoveRankEntry
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminRemoveRankEntryGrpcPath = "/rankpb.RankAdmin/RemoveRankEntry";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendRankAdminRemoveRankEntryErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region RankAdminDeleteRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminDeleteRankGenerationGrpcPath = "/rankpb.RankAdmin/DeleteRankGeneration";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendRankAdminDeleteRankGenerationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetRankAdminCallDeadline(std::chrono::milliseconds deadline);

}// namespace rankpb
