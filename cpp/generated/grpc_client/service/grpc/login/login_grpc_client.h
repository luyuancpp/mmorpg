#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/service/grpc/login/login.grpc.pb.h"


#include "rpc/service_metadata/login_service_metadata.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace loginpb {
using ClientPlayerLoginStubPtr = std::unique_ptr<ClientPlayerLogin::Stub>;
#pragma region ClientPlayerLoginLogin


struct AsyncClientPlayerLoginLoginGrpcClient {
    uint32_t messageId{ ClientPlayerLoginLoginMessageId };
    ClientContext context;
    Status status;
    ::loginpb::LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginResponse>> response_reader;
};



class ::loginpb::LoginRequest;
using AsyncClientPlayerLoginLoginHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::LoginResponse&)>;
extern AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region ClientPlayerLoginCreatePlayer


struct AsyncClientPlayerLoginCreatePlayerGrpcClient {
    uint32_t messageId{ ClientPlayerLoginCreatePlayerMessageId };
    ClientContext context;
    Status status;
    ::loginpb::CreatePlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::CreatePlayerResponse>> response_reader;
};



class ::loginpb::CreatePlayerRequest;
using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::CreatePlayerResponse&)>;
extern AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region ClientPlayerLoginEnterGame


struct AsyncClientPlayerLoginEnterGameGrpcClient {
    uint32_t messageId{ ClientPlayerLoginEnterGameMessageId };
    ClientContext context;
    Status status;
    ::loginpb::EnterGameResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::EnterGameResponse>> response_reader;
};



class ::loginpb::EnterGameRequest;
using AsyncClientPlayerLoginEnterGameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::EnterGameResponse&)>;
extern AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region ClientPlayerLoginLeaveGame


struct AsyncClientPlayerLoginLeaveGameGrpcClient {
    uint32_t messageId{ ClientPlayerLoginLeaveGameMessageId };
    ClientContext context;
    Status status;
    ::loginpb::LoginEmptyResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginEmptyResponse>> response_reader;
};



class ::loginpb::LeaveGameRequest;
using AsyncClientPlayerLoginLeaveGameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::LoginEmptyResponse&)>;
extern AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region ClientPlayerLoginDisconnect


struct AsyncClientPlayerLoginDisconnectGrpcClient {
    uint32_t messageId{ ClientPlayerLoginDisconnectMessageId };
    ClientContext context;
    Status status;
    ::loginpb::LoginEmptyResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginEmptyResponse>> response_reader;
};



class ::loginpb::LoginNodeDisconnectRequest;
using AsyncClientPlayerLoginDisconnectHandlerFunctionType =
    std::function<void(const ClientContext&, const ::loginpb::LoginEmptyResponse&)>;
extern AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetLoginHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetLoginIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleLoginCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitLoginGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace loginpb

