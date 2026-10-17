#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/rank_service_metadata.h"

namespace rankpb {
#pragma region ClientPlayerRankGetRankPage
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerRankGetRankPageGrpcPath = "/rankpb.ClientPlayerRank/GetRankPage";
using AsyncClientPlayerRankGetRankPageHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::GetRankPageResponse&)>;
extern AsyncClientPlayerRankGetRankPageHandlerFunctionType AsyncClientPlayerRankGetRankPageHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerRankGetRankPageFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::GetRankPageRequest&)>;
extern AsyncClientPlayerRankGetRankPageFailedHandlerFunctionType AsyncClientPlayerRankGetRankPageFailedHandler;

void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request);
void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerRankGetRankPage(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::GetRankPageRequest& request, GrpcDone<::rankpb::GetRankPageResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetRankHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetRankIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetRankFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetRankIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetRankCallDeadline(std::chrono::milliseconds deadline);

}// namespace rankpb
