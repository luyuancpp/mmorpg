#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/rank_admin_service_metadata.h"

namespace rankpb {
#pragma region RankAdminOpenRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminOpenRankGenerationGrpcPath = "/rankpb.RankAdmin/OpenRankGeneration";
using AsyncRankAdminOpenRankGenerationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::OpenRankGenerationResponse&)>;
extern AsyncRankAdminOpenRankGenerationHandlerFunctionType AsyncRankAdminOpenRankGenerationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncRankAdminOpenRankGenerationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::OpenRankGenerationRequest&)>;
extern AsyncRankAdminOpenRankGenerationFailedHandlerFunctionType AsyncRankAdminOpenRankGenerationFailedHandler;

void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request);
void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request, GrpcDone<::rankpb::OpenRankGenerationResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region RankAdminSubmitRankScores
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminSubmitRankScoresGrpcPath = "/rankpb.RankAdmin/SubmitRankScores";
using AsyncRankAdminSubmitRankScoresHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::SubmitRankScoresResponse&)>;
extern AsyncRankAdminSubmitRankScoresHandlerFunctionType AsyncRankAdminSubmitRankScoresHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncRankAdminSubmitRankScoresFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::SubmitRankScoresRequest&)>;
extern AsyncRankAdminSubmitRankScoresFailedHandlerFunctionType AsyncRankAdminSubmitRankScoresFailedHandler;

void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request);
void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request, GrpcDone<::rankpb::SubmitRankScoresResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region RankAdminFreezeRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminFreezeRankGenerationGrpcPath = "/rankpb.RankAdmin/FreezeRankGeneration";
using AsyncRankAdminFreezeRankGenerationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::FreezeRankGenerationResponse&)>;
extern AsyncRankAdminFreezeRankGenerationHandlerFunctionType AsyncRankAdminFreezeRankGenerationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncRankAdminFreezeRankGenerationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::FreezeRankGenerationRequest&)>;
extern AsyncRankAdminFreezeRankGenerationFailedHandlerFunctionType AsyncRankAdminFreezeRankGenerationFailedHandler;

void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request);
void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request, GrpcDone<::rankpb::FreezeRankGenerationResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region RankAdminRemoveRankEntry
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminRemoveRankEntryGrpcPath = "/rankpb.RankAdmin/RemoveRankEntry";
using AsyncRankAdminRemoveRankEntryHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::RankAdminResponse&)>;
extern AsyncRankAdminRemoveRankEntryHandlerFunctionType AsyncRankAdminRemoveRankEntryHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncRankAdminRemoveRankEntryFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::RemoveRankEntryRequest&)>;
extern AsyncRankAdminRemoveRankEntryFailedHandlerFunctionType AsyncRankAdminRemoveRankEntryFailedHandler;

void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request);
void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region RankAdminDeleteRankGeneration
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kRankAdminDeleteRankGenerationGrpcPath = "/rankpb.RankAdmin/DeleteRankGeneration";
using AsyncRankAdminDeleteRankGenerationHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::rankpb::RankAdminResponse&)>;
extern AsyncRankAdminDeleteRankGenerationHandlerFunctionType AsyncRankAdminDeleteRankGenerationHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncRankAdminDeleteRankGenerationFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::rankpb::RemoveRankEntryRequest&)>;
extern AsyncRankAdminDeleteRankGenerationFailedHandlerFunctionType AsyncRankAdminDeleteRankGenerationFailedHandler;

void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request);
void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetRankAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetRankAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetRankAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetRankAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetRankAdminCallDeadline(std::chrono::milliseconds deadline);

}// namespace rankpb
