#pragma once
#include <chrono>
#include <string_view>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/jubaozhai_service_metadata.h"

namespace trade {
#pragma region ClientPlayerJubaozhaiBrowseListings
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBrowseListingsGrpcPath = "/trade.ClientPlayerJubaozhai/BrowseListings";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request, GrpcDone<::trade::BrowseListingsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiBrowseListingsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetListingDetail
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetListingDetailGrpcPath = "/trade.ClientPlayerJubaozhai/GetListingDetail";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request, GrpcDone<::trade::GetListingDetailResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiGetListingDetailErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiSetFavorite
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiSetFavoriteGrpcPath = "/trade.ClientPlayerJubaozhai/SetFavorite";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request, GrpcDone<::trade::SetFavoriteResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiSetFavoriteErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMyShelf
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetMyShelfGrpcPath = "/trade.ClientPlayerJubaozhai/GetMyShelf";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request, GrpcDone<::trade::GetMyShelfResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiGetMyShelfErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiListOfficialSkus
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListOfficialSkusGrpcPath = "/trade.ClientPlayerJubaozhai/ListOfficialSkus";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request, GrpcDone<::trade::ListOfficialSkusResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiListOfficialSkusErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateOfficialOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateOfficialOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CreateOfficialOrder";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCreateOfficialOrderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetCommerceOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetCommerceOrderGrpcPath = "/trade.ClientPlayerJubaozhai/GetCommerceOrder";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiGetCommerceOrderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelCommerceOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelCommerceOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CancelCommerceOrder";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCancelCommerceOrderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiListCommerceOrders
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListCommerceOrdersGrpcPath = "/trade.ClientPlayerJubaozhai/ListCommerceOrders";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request, GrpcDone<::trade::ListCommerceOrdersResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiListCommerceOrdersErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreatePlayerTrade
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreatePlayerTradeGrpcPath = "/trade.ClientPlayerJubaozhai/CreatePlayerTrade";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCreatePlayerTradeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiConfirmPlayerTrade
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiConfirmPlayerTradeGrpcPath = "/trade.ClientPlayerJubaozhai/ConfirmPlayerTrade";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiConfirmPlayerTradeErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/CreateConsignment";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCreateConsignmentErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiBuyConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBuyConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/BuyConsignment";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiBuyConsignmentErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelConsignment
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelConsignmentGrpcPath = "/trade.ClientPlayerJubaozhai/CancelConsignment";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request, GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCancelConsignmentErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceMarketOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiPlaceMarketOrderGrpcPath = "/trade.ClientPlayerJubaozhai/PlaceMarketOrder";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request, GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiPlaceMarketOrderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelMarketOrder
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelMarketOrderGrpcPath = "/trade.ClientPlayerJubaozhai/CancelMarketOrder";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request, GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCancelMarketOrderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiListMarketOrders
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListMarketOrdersGrpcPath = "/trade.ClientPlayerJubaozhai/ListMarketOrders";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request, GrpcDone<::trade::ListMarketOrdersResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiListMarketOrdersErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMarketDepth
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetMarketDepthGrpcPath = "/trade.ClientPlayerJubaozhai/GetMarketDepth";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request, GrpcDone<::trade::GetMarketDepthResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiGetMarketDepthErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCreateAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/CreateAuctionLot";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCreateAuctionLotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceAuctionBid
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiPlaceAuctionBidGrpcPath = "/trade.ClientPlayerJubaozhai/PlaceAuctionBid";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiPlaceAuctionBidErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiGetAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/GetAuctionLot";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiGetAuctionLotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelAuctionLot
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiCancelAuctionLotGrpcPath = "/trade.ClientPlayerJubaozhai/CancelAuctionLot";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request, GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiCancelAuctionLotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiBrowseCommerceOffers
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiBrowseCommerceOffersGrpcPath = "/trade.ClientPlayerJubaozhai/BrowseCommerceOffers";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request, GrpcDone<::trade::BrowseCommerceOffersResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiBrowseCommerceOffersErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
#pragma region ClientPlayerJubaozhaiListTradeMarkets
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerJubaozhaiListTradeMarketsGrpcPath = "/trade.ClientPlayerJubaozhai/ListTradeMarkets";
// done 必填且恰好调用一次,从不在 Send 栈内同步调用;为空 LOG_FATAL。响应只在回调期间有效。
void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request, GrpcDone<::trade::ListTradeMarketsResponse> done, const GrpcCallOptions& options = {});
// 类型擦除版只供元数据 sender 使用;动态类型不匹配时异步回 INTERNAL,不发出请求。
void SendClientPlayerJubaozhaiListTradeMarketsErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, GrpcMessageDone done, const GrpcCallOptions& options);
#pragma endregion
// 本文件所有 unary 调用此后使用的 deadline;启动时按目标节点类型设置。
void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline);

}// namespace trade
