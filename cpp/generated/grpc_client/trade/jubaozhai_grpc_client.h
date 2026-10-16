#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/jubaozhai_service_metadata.h"

namespace trade {
#pragma region ClientPlayerJubaozhaiBrowseListings
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBrowseListingsGrpcPath = "/trade.ClientPlayerJubaozhai/BrowseListings";
using AsyncClientPlayerJubaozhaiBrowseListingsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::BrowseListingsResponse&)>;
extern AsyncClientPlayerJubaozhaiBrowseListingsHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiBrowseListingsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::BrowseListingsRequest&)>;
extern AsyncClientPlayerJubaozhaiBrowseListingsFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler;

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request);
void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request, GrpcDone<::trade::BrowseListingsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetListingDetail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetListingDetailGrpcPath = "/trade.ClientPlayerJubaozhai/GetListingDetail";
using AsyncClientPlayerJubaozhaiGetListingDetailHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::GetListingDetailResponse&)>;
extern AsyncClientPlayerJubaozhaiGetListingDetailHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiGetListingDetailFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::GetListingDetailRequest&)>;
extern AsyncClientPlayerJubaozhaiGetListingDetailFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler;

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request);
void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request, GrpcDone<::trade::GetListingDetailResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiSetFavorite
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiSetFavoriteGrpcPath = "/trade.ClientPlayerJubaozhai/SetFavorite";
using AsyncClientPlayerJubaozhaiSetFavoriteHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::SetFavoriteResponse&)>;
extern AsyncClientPlayerJubaozhaiSetFavoriteHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiSetFavoriteFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::SetFavoriteRequest&)>;
extern AsyncClientPlayerJubaozhaiSetFavoriteFailedHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler;

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request);
void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request, GrpcDone<::trade::SetFavoriteResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMyShelf
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetMyShelfGrpcPath = "/trade.ClientPlayerJubaozhai/GetMyShelf";
using AsyncClientPlayerJubaozhaiGetMyShelfHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::GetMyShelfResponse&)>;
extern AsyncClientPlayerJubaozhaiGetMyShelfHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiGetMyShelfFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::GetMyShelfRequest&)>;
extern AsyncClientPlayerJubaozhaiGetMyShelfFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler;

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request);
void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request, GrpcDone<::trade::GetMyShelfResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetJubaozhaiHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetJubaozhaiIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetJubaozhaiIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline);

}// namespace trade
