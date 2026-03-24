#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/friend/friend.grpc.pb.h"

#include "rpc/service_metadata/friend_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace friendpb {
using FriendServiceStubPtr = std::unique_ptr<FriendService::Stub>;
#pragma region FriendServiceAddFriend

struct AsyncFriendServiceAddFriendGrpcClient {
    uint32_t messageId{ FriendServiceAddFriendMessageId };
    ClientContext context;
    Status status;
    ::friendpb::AddFriendResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::AddFriendResponse>> response_reader;
};

class ::friendpb::AddFriendRequest;
using AsyncFriendServiceAddFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::AddFriendResponse&)>;
extern AsyncFriendServiceAddFriendHandlerFunctionType AsyncFriendServiceAddFriendHandler;

void SendFriendServiceAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request);
void SendFriendServiceAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceAddFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region FriendServiceAcceptFriend

struct AsyncFriendServiceAcceptFriendGrpcClient {
    uint32_t messageId{ FriendServiceAcceptFriendMessageId };
    ClientContext context;
    Status status;
    ::friendpb::AcceptFriendResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::AcceptFriendResponse>> response_reader;
};

class ::friendpb::AcceptFriendRequest;
using AsyncFriendServiceAcceptFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::AcceptFriendResponse&)>;
extern AsyncFriendServiceAcceptFriendHandlerFunctionType AsyncFriendServiceAcceptFriendHandler;

void SendFriendServiceAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request);
void SendFriendServiceAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region FriendServiceRejectFriend

struct AsyncFriendServiceRejectFriendGrpcClient {
    uint32_t messageId{ FriendServiceRejectFriendMessageId };
    ClientContext context;
    Status status;
    ::friendpb::RejectFriendResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::RejectFriendResponse>> response_reader;
};

class ::friendpb::RejectFriendRequest;
using AsyncFriendServiceRejectFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::RejectFriendResponse&)>;
extern AsyncFriendServiceRejectFriendHandlerFunctionType AsyncFriendServiceRejectFriendHandler;

void SendFriendServiceRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request);
void SendFriendServiceRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceRejectFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region FriendServiceRemoveFriend

struct AsyncFriendServiceRemoveFriendGrpcClient {
    uint32_t messageId{ FriendServiceRemoveFriendMessageId };
    ClientContext context;
    Status status;
    ::friendpb::RemoveFriendResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::RemoveFriendResponse>> response_reader;
};

class ::friendpb::RemoveFriendRequest;
using AsyncFriendServiceRemoveFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::RemoveFriendResponse&)>;
extern AsyncFriendServiceRemoveFriendHandlerFunctionType AsyncFriendServiceRemoveFriendHandler;

void SendFriendServiceRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request);
void SendFriendServiceRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region FriendServiceGetFriendList

struct AsyncFriendServiceGetFriendListGrpcClient {
    uint32_t messageId{ FriendServiceGetFriendListMessageId };
    ClientContext context;
    Status status;
    ::friendpb::GetFriendListResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::GetFriendListResponse>> response_reader;
};

class ::friendpb::GetFriendListRequest;
using AsyncFriendServiceGetFriendListHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::GetFriendListResponse&)>;
extern AsyncFriendServiceGetFriendListHandlerFunctionType AsyncFriendServiceGetFriendListHandler;

void SendFriendServiceGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request);
void SendFriendServiceGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceGetFriendList(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region FriendServiceGetPendingRequests

struct AsyncFriendServiceGetPendingRequestsGrpcClient {
    uint32_t messageId{ FriendServiceGetPendingRequestsMessageId };
    ClientContext context;
    Status status;
    ::friendpb::GetPendingRequestsResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::friendpb::GetPendingRequestsResponse>> response_reader;
};

class ::friendpb::GetPendingRequestsRequest;
using AsyncFriendServiceGetPendingRequestsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::GetPendingRequestsResponse&)>;
extern AsyncFriendServiceGetPendingRequestsHandlerFunctionType AsyncFriendServiceGetPendingRequestsHandler;

void SendFriendServiceGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request);
void SendFriendServiceGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendFriendServiceGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetFriendHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetFriendIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleFriendCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitFriendGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace friendpb
