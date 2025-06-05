#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/login/login_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace loginpb {
using ClientPlayerLoginStubPtr = std::unique_ptr<ClientPlayerLogin::Stub>;
#pragma region ClientPlayerLoginLogin

class AsyncClientPlayerLoginLoginGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginResponse>> response_reader;
};




class ::loginpb::LoginRequest;
using AsyncClientPlayerLoginLoginHandlerFunctionType = std::function<void(const ClientContext&, const ::loginpb::LoginResponse&)>;
extern AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

#pragma region ClientPlayerLoginCreatePlayer

class AsyncClientPlayerLoginCreatePlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::CreatePlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::CreatePlayerResponse>> response_reader;
};




class ::loginpb::CreatePlayerRequest;
using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType = std::function<void(const ClientContext&, const ::loginpb::CreatePlayerResponse&)>;
extern AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

#pragma region ClientPlayerLoginEnterGame

class AsyncClientPlayerLoginEnterGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::EnterGameResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::EnterGameResponse>> response_reader;
};




class ::loginpb::EnterGameRequest;
using AsyncClientPlayerLoginEnterGameHandlerFunctionType = std::function<void(const ClientContext&, const ::loginpb::EnterGameResponse&)>;
extern AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

#pragma region ClientPlayerLoginLeaveGame

class AsyncClientPlayerLoginLeaveGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};




class ::loginpb::LeaveGameRequest;
using AsyncClientPlayerLoginLeaveGameHandlerFunctionType = std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

#pragma region ClientPlayerLoginDisconnect

class AsyncClientPlayerLoginDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};




class ::loginpb::LoginNodeDisconnectRequest;
using AsyncClientPlayerLoginDisconnectHandlerFunctionType = std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion

void SetLoginServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);


}// namespace loginpb
