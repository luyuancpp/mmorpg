#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/rank_service_metadata.h"

namespace rankpb {
#pragma region ClientPlayerRankGetRankPage
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerRankGetRankPageGrpcPath = "/rankpb.ClientPlayerRank/GetRankPage";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request, GrpcDone<::rankpb::GetRankPageResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerRankGetRankPageErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerRankGetRankAround
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerRankGetRankAroundGrpcPath = "/rankpb.ClientPlayerRank/GetRankAround";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerRankGetRankAround(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankAroundRequest& request, GrpcDone<::rankpb::GetRankAroundResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerRankGetRankAroundErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetRankCallDeadline(std::chrono::milliseconds deadline);

}// namespace rankpb
