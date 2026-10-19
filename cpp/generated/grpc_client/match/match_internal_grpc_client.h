#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "core/network/grpc_call_types.h"

#include "rpc/service_metadata/match_internal_service_metadata.h"

namespace match {
#pragma region MatchInternalStartActivityBattle
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kMatchInternalStartActivityBattleGrpcPath = "/match.MatchInternal/StartActivityBattle";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendMatchInternalStartActivityBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::StartActivityBattleRequest& request, GrpcDone<::match::StartActivityBattleResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendMatchInternalStartActivityBattleErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetMatchInternalCallDeadline(std::chrono::milliseconds deadline);

}// namespace match
