#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "entt/src/entt/entity/registry.hpp"
#include "grpc_client/grpc_call_tag.h"

#include "rpc/service_metadata/friend_service_metadata.h"

namespace friendpb {
#pragma region ClientPlayerFriendAddFriend
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendAddFriendGrpcPath = "/friendpb.ClientPlayerFriend/AddFriend";
using AsyncClientPlayerFriendAddFriendHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::AddFriendResponse&)>;
extern AsyncClientPlayerFriendAddFriendHandlerFunctionType AsyncClientPlayerFriendAddFriendHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendAddFriendFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::AddFriendRequest&)>;
extern AsyncClientPlayerFriendAddFriendFailedHandlerFunctionType AsyncClientPlayerFriendAddFriendFailedHandler;

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request);
void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request, GrpcDone<::friendpb::AddFriendResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendAcceptFriend
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendAcceptFriendGrpcPath = "/friendpb.ClientPlayerFriend/AcceptFriend";
using AsyncClientPlayerFriendAcceptFriendHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::AcceptFriendResponse&)>;
extern AsyncClientPlayerFriendAcceptFriendHandlerFunctionType AsyncClientPlayerFriendAcceptFriendHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendAcceptFriendFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::AcceptFriendRequest&)>;
extern AsyncClientPlayerFriendAcceptFriendFailedHandlerFunctionType AsyncClientPlayerFriendAcceptFriendFailedHandler;

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request);
void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request, GrpcDone<::friendpb::AcceptFriendResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendRejectFriend
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendRejectFriendGrpcPath = "/friendpb.ClientPlayerFriend/RejectFriend";
using AsyncClientPlayerFriendRejectFriendHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::RejectFriendResponse&)>;
extern AsyncClientPlayerFriendRejectFriendHandlerFunctionType AsyncClientPlayerFriendRejectFriendHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendRejectFriendFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::RejectFriendRequest&)>;
extern AsyncClientPlayerFriendRejectFriendFailedHandlerFunctionType AsyncClientPlayerFriendRejectFriendFailedHandler;

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request);
void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request, GrpcDone<::friendpb::RejectFriendResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendRemoveFriend
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendRemoveFriendGrpcPath = "/friendpb.ClientPlayerFriend/RemoveFriend";
using AsyncClientPlayerFriendRemoveFriendHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::RemoveFriendResponse&)>;
extern AsyncClientPlayerFriendRemoveFriendHandlerFunctionType AsyncClientPlayerFriendRemoveFriendHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendRemoveFriendFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::RemoveFriendRequest&)>;
extern AsyncClientPlayerFriendRemoveFriendFailedHandlerFunctionType AsyncClientPlayerFriendRemoveFriendFailedHandler;

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request);
void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request, GrpcDone<::friendpb::RemoveFriendResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendGetFriendList
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendGetFriendListGrpcPath = "/friendpb.ClientPlayerFriend/GetFriendList";
using AsyncClientPlayerFriendGetFriendListHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::GetFriendListResponse&)>;
extern AsyncClientPlayerFriendGetFriendListHandlerFunctionType AsyncClientPlayerFriendGetFriendListHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendGetFriendListFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::GetFriendListRequest&)>;
extern AsyncClientPlayerFriendGetFriendListFailedHandlerFunctionType AsyncClientPlayerFriendGetFriendListFailedHandler;

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request);
void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request, GrpcDone<::friendpb::GetFriendListResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendGetPendingRequests
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendGetPendingRequestsGrpcPath = "/friendpb.ClientPlayerFriend/GetPendingRequests";
using AsyncClientPlayerFriendGetPendingRequestsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::GetPendingRequestsResponse&)>;
extern AsyncClientPlayerFriendGetPendingRequestsHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendGetPendingRequestsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::GetPendingRequestsRequest&)>;
extern AsyncClientPlayerFriendGetPendingRequestsFailedHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsFailedHandler;

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request);
void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request, GrpcDone<::friendpb::GetPendingRequestsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendBlock
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendBlockGrpcPath = "/friendpb.ClientPlayerFriend/Block";
using AsyncClientPlayerFriendBlockHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::BlockResponse&)>;
extern AsyncClientPlayerFriendBlockHandlerFunctionType AsyncClientPlayerFriendBlockHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendBlockFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::BlockRequest&)>;
extern AsyncClientPlayerFriendBlockFailedHandlerFunctionType AsyncClientPlayerFriendBlockFailedHandler;

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request);
void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request, GrpcDone<::friendpb::BlockResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendUnblock
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendUnblockGrpcPath = "/friendpb.ClientPlayerFriend/Unblock";
using AsyncClientPlayerFriendUnblockHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::UnblockResponse&)>;
extern AsyncClientPlayerFriendUnblockHandlerFunctionType AsyncClientPlayerFriendUnblockHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendUnblockFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::UnblockRequest&)>;
extern AsyncClientPlayerFriendUnblockFailedHandlerFunctionType AsyncClientPlayerFriendUnblockFailedHandler;

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request);
void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request, GrpcDone<::friendpb::UnblockResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendListBlocks
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendListBlocksGrpcPath = "/friendpb.ClientPlayerFriend/ListBlocks";
using AsyncClientPlayerFriendListBlocksHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::ListBlocksResponse&)>;
extern AsyncClientPlayerFriendListBlocksHandlerFunctionType AsyncClientPlayerFriendListBlocksHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendListBlocksFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::ListBlocksRequest&)>;
extern AsyncClientPlayerFriendListBlocksFailedHandlerFunctionType AsyncClientPlayerFriendListBlocksFailedHandler;

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request);
void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request, GrpcDone<::friendpb::ListBlocksResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendRecommendFriends
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendRecommendFriendsGrpcPath = "/friendpb.ClientPlayerFriend/RecommendFriends";
using AsyncClientPlayerFriendRecommendFriendsHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::friendpb::RecommendFriendsResponse&)>;
extern AsyncClientPlayerFriendRecommendFriendsHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendRecommendFriendsFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::RecommendFriendsRequest&)>;
extern AsyncClientPlayerFriendRecommendFriendsFailedHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsFailedHandler;

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request);
void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request, GrpcDone<::friendpb::RecommendFriendsResponse> done, const GrpcCallOptions& options = {});
#pragma endregion
#pragma region ClientPlayerFriendNotifyFriendEvent
// gRPC :path(线上契约,与 Go 侧一致)。
inline constexpr std::string_view kClientPlayerFriendNotifyFriendEventGrpcPath = "/friendpb.ClientPlayerFriend/NotifyFriendEvent";
using AsyncClientPlayerFriendNotifyFriendEventHandlerFunctionType =
    std::function<void(const GrpcReplyContext&, const ::Empty&)>;
extern AsyncClientPlayerFriendNotifyFriendEventHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventHandler;
// 调用以非 OK 状态结束时的回调(deadline 到期 / 连接不可用 / 服务端报错),拿到发出的请求副本。
// 未装时只打 ERROR 日志。失败 = 结果未知,语义见 GrpcCallFailure。
using AsyncClientPlayerFriendNotifyFriendEventFailedHandlerFunctionType =
    std::function<void(const GrpcCallFailure&, const ::friendpb::FriendEventS2C&)>;
extern AsyncClientPlayerFriendNotifyFriendEventFailedHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventFailedHandler;

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request);
void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
// 每次调用回调版(契约见 grpc_call_tag.h 的 GrpcDone):done 恰好调用一次、从不在本函数栈内同步调用,
// 且本次调用不再触发上面的全局处理器。done 为空等同于不带回调的 Send。
void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request, GrpcDone<::Empty> done, const GrpcCallOptions& options = {});
#pragma endregion
void SetFriendHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetFriendIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler);
void SetFriendFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
void SetFriendIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler);
// 本文件所有 unary 调用此后使用的 deadline;启动时由 SetGrpcCallDeadline 按目标节点类型调用。
void SetFriendCallDeadline(std::chrono::milliseconds deadline);

}// namespace friendpb
