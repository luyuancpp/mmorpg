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
    ::loginpb::LoginC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginC2LResponse>> response_reader;
};

using AsyncClientPlayerLoginLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginLoginGrpcClientCall>&)>;
extern AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;


class ::loginpb::LoginC2LRequest;

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry);
void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region ClientPlayerLoginCreatePlayer

class AsyncClientPlayerLoginCreatePlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::CreatePlayerC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::CreatePlayerC2LResponse>> response_reader;
};

using AsyncClientPlayerLoginCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginCreatePlayerGrpcClientCall>&)>;
extern AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;


class ::loginpb::CreatePlayerC2LRequest;

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry);
void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region ClientPlayerLoginEnterGame

class AsyncClientPlayerLoginEnterGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::EnterGameC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::EnterGameC2LResponse>> response_reader;
};

using AsyncClientPlayerLoginEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginEnterGameGrpcClientCall>&)>;
extern AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;


class ::loginpb::EnterGameC2LRequest;

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry);
void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region ClientPlayerLoginLeaveGame

class AsyncClientPlayerLoginLeaveGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncClientPlayerLoginLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginLeaveGameGrpcClientCall>&)>;
extern AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;


class ::loginpb::LeaveGameC2LRequest;

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry);
void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region ClientPlayerLoginDisconnect

class AsyncClientPlayerLoginDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncClientPlayerLoginDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncClientPlayerLoginDisconnectGrpcClientCall>&)>;
extern AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;


class ::loginpb::LoginNodeDisconnectRequest;

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleClientPlayerLoginCompletedQueueMessage(entt::registry& registry);
void InitClientPlayerLoginCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion



}// namespace loginpb
