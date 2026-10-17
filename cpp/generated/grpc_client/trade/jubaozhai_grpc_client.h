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
#pragma region ClientPlayerJubaozhaiListOfficialSkus
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListOfficialSkusGrpcPath = "/trade.ClientPlayerJubaozhai/ListOfficialSkus";
using AsyncClientPlayerJubaozhaiListOfficialSkusHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::ListOfficialSkusResponse&)>;
extern AsyncClientPlayerJubaozhaiListOfficialSkusHandlerFunctionType AsyncClientPlayerJubaozhaiListOfficialSkusHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::ListOfficialSkusRequest&)>;
extern AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler;

void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request);
void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request, GrpcDone<::trade::ListOfficialSkusResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateOfficialOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateOfficialOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CreateOfficialOrder";
using AsyncClientPlayerJubaozhaiCreateOfficialOrderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCreateOfficialOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CreateOfficialOrderRequest&)>;
extern AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler;

void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request);
void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetCommerceOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetCommerceOrderGrpcPath = "/trade.ClientPlayerJubaozhai/GetCommerceOrder";
using AsyncClientPlayerJubaozhaiGetCommerceOrderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiGetCommerceOrderHandlerFunctionType AsyncClientPlayerJubaozhaiGetCommerceOrderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::GetCommerceOrderRequest&)>;
extern AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler;

void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request);
void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelCommerceOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelCommerceOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CancelCommerceOrder";
using AsyncClientPlayerJubaozhaiCancelCommerceOrderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCancelCommerceOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CancelCommerceOrderRequest&)>;
extern AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler;

void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request);
void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiListCommerceOrders
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListCommerceOrdersGrpcPath = "/trade.ClientPlayerJubaozhai/ListCommerceOrders";
using AsyncClientPlayerJubaozhaiListCommerceOrdersHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::ListCommerceOrdersResponse&)>;
extern AsyncClientPlayerJubaozhaiListCommerceOrdersHandlerFunctionType AsyncClientPlayerJubaozhaiListCommerceOrdersHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::ListCommerceOrdersRequest&)>;
extern AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler;

void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request);
void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request, GrpcDone<::trade::ListCommerceOrdersResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreatePlayerTrade
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreatePlayerTradeGrpcPath = "/trade.ClientPlayerJubaozhai/CreatePlayerTrade";
using AsyncClientPlayerJubaozhaiCreatePlayerTradeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCreatePlayerTradeHandlerFunctionType AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CreatePlayerTradeRequest&)>;
extern AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler;

void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request);
void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiConfirmPlayerTrade
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiConfirmPlayerTradeGrpcPath = "/trade.ClientPlayerJubaozhai/ConfirmPlayerTrade";
using AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandlerFunctionType AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::ConfirmPlayerTradeRequest&)>;
extern AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandlerFunctionType AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler;

void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request);
void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/CreateConsignment";
using AsyncClientPlayerJubaozhaiCreateConsignmentHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCreateConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiCreateConsignmentHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CreateConsignmentRequest&)>;
extern AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler;

void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request);
void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiBuyConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBuyConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/BuyConsignment";
using AsyncClientPlayerJubaozhaiBuyConsignmentHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiBuyConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiBuyConsignmentHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::BuyConsignmentRequest&)>;
extern AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler;

void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request);
void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/CancelConsignment";
using AsyncClientPlayerJubaozhaiCancelConsignmentHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::CommerceOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCancelConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiCancelConsignmentHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CancelConsignmentRequest&)>;
extern AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler;

void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request);
void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceMarketOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiPlaceMarketOrderGrpcPath = "/trade.ClientPlayerJubaozhai/PlaceMarketOrder";
using AsyncClientPlayerJubaozhaiPlaceMarketOrderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::MarketOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiPlaceMarketOrderHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::PlaceMarketOrderRequest&)>;
extern AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler;

void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request);
void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request, GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelMarketOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelMarketOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CancelMarketOrder";
using AsyncClientPlayerJubaozhaiCancelMarketOrderHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::MarketOrderResponse&)>;
extern AsyncClientPlayerJubaozhaiCancelMarketOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCancelMarketOrderHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CancelMarketOrderRequest&)>;
extern AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler;

void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request);
void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request, GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiListMarketOrders
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListMarketOrdersGrpcPath = "/trade.ClientPlayerJubaozhai/ListMarketOrders";
using AsyncClientPlayerJubaozhaiListMarketOrdersHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::ListMarketOrdersResponse&)>;
extern AsyncClientPlayerJubaozhaiListMarketOrdersHandlerFunctionType AsyncClientPlayerJubaozhaiListMarketOrdersHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::ListMarketOrdersRequest&)>;
extern AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler;

void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request);
void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request, GrpcDone<::trade::ListMarketOrdersResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMarketDepth
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetMarketDepthGrpcPath = "/trade.ClientPlayerJubaozhai/GetMarketDepth";
using AsyncClientPlayerJubaozhaiGetMarketDepthHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::GetMarketDepthResponse&)>;
extern AsyncClientPlayerJubaozhaiGetMarketDepthHandlerFunctionType AsyncClientPlayerJubaozhaiGetMarketDepthHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::GetMarketDepthRequest&)>;
extern AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler;

void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request);
void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request, GrpcDone<::trade::GetMarketDepthResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/CreateAuctionLot";
using AsyncClientPlayerJubaozhaiCreateAuctionLotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::AuctionLotResponse&)>;
extern AsyncClientPlayerJubaozhaiCreateAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiCreateAuctionLotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CreateAuctionLotRequest&)>;
extern AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler;

void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request);
void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceAuctionBid
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiPlaceAuctionBidGrpcPath = "/trade.ClientPlayerJubaozhai/PlaceAuctionBid";
using AsyncClientPlayerJubaozhaiPlaceAuctionBidHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::AuctionLotResponse&)>;
extern AsyncClientPlayerJubaozhaiPlaceAuctionBidHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::PlaceAuctionBidRequest&)>;
extern AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler;

void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request);
void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/GetAuctionLot";
using AsyncClientPlayerJubaozhaiGetAuctionLotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::AuctionLotResponse&)>;
extern AsyncClientPlayerJubaozhaiGetAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiGetAuctionLotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::GetAuctionLotRequest&)>;
extern AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler;

void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request);
void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/CancelAuctionLot";
using AsyncClientPlayerJubaozhaiCancelAuctionLotHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::AuctionLotResponse&)>;
extern AsyncClientPlayerJubaozhaiCancelAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiCancelAuctionLotHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::CancelAuctionLotRequest&)>;
extern AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler;

void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request);
void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiBrowseCommerceOffers
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBrowseCommerceOffersGrpcPath = "/trade.ClientPlayerJubaozhai/BrowseCommerceOffers";
using AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::BrowseCommerceOffersResponse&)>;
extern AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::BrowseCommerceOffersRequest&)>;
extern AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler;

void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request);
void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request, GrpcDone<::trade::BrowseCommerceOffersResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerJubaozhaiListTradeMarkets
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListTradeMarketsGrpcPath = "/trade.ClientPlayerJubaozhai/ListTradeMarkets";
using AsyncClientPlayerJubaozhaiListTradeMarketsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::trade::ListTradeMarketsResponse&)>;
extern AsyncClientPlayerJubaozhaiListTradeMarketsHandlerFunctionType AsyncClientPlayerJubaozhaiListTradeMarketsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::trade::ListTradeMarketsRequest&)>;
extern AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler;

void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request);
void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request, GrpcDone<::trade::ListTradeMarketsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetJubaozhaiHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetJubaozhaiIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetJubaozhaiIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline);

}// namespace trade
