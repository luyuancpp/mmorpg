#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/login/login_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace loginpb {
using LoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;
#pragma region LoginServiceLogin

class AsyncLoginServiceLoginGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::LoginC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginC2LResponse>> response_reader;
};

using AsyncLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;
extern AsyncLoginServiceLoginHandlerFunctionType AsyncLoginServiceLoginHandler;


class ::loginpb::LoginC2LRequest;

void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request);
void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region LoginServiceCreatePlayer

class AsyncLoginServiceCreatePlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::CreatePlayerC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::CreatePlayerC2LResponse>> response_reader;
};

using AsyncLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;
extern AsyncLoginServiceCreatePlayerHandlerFunctionType AsyncLoginServiceCreatePlayerHandler;


class ::loginpb::CreatePlayerC2LRequest;

void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request);
void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region LoginServiceEnterGame

class AsyncLoginServiceEnterGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::EnterGameC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::EnterGameC2LResponse>> response_reader;
};

using AsyncLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;
extern AsyncLoginServiceEnterGameHandlerFunctionType AsyncLoginServiceEnterGameHandler;


class ::loginpb::EnterGameC2LRequest;

void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request);
void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region LoginServiceLeaveGame

class AsyncLoginServiceLeaveGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;
extern AsyncLoginServiceLeaveGameHandlerFunctionType AsyncLoginServiceLeaveGameHandler;


class ::loginpb::LeaveGameC2LRequest;

void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request);
void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region LoginServiceDisconnect

class AsyncLoginServiceDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)>;
extern AsyncLoginServiceDisconnectHandlerFunctionType AsyncLoginServiceDisconnectHandler;


class ::loginpb::LoginNodeDisconnectRequest;

void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion



}// namespace loginpb
