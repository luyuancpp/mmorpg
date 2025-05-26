#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/login_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace loginpb {

using GrpcloginpbLoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;



class AsyncloginpbLoginServiceLoginGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::LoginC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::LoginC2LResponse>> response_reader;
};

using AsyncloginpbLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceLoginGrpcClientCall>&)>;
extern AsyncloginpbLoginServiceLoginHandlerFunctionType AsyncloginpbLoginServiceLoginHandler;



class ::loginpb::LoginC2LRequest;

void SendloginpbLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginC2LRequest& request);
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncloginpbLoginServiceCreatePlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::CreatePlayerC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::CreatePlayerC2LResponse>> response_reader;
};

using AsyncloginpbLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceCreatePlayerGrpcClientCall>&)>;
extern AsyncloginpbLoginServiceCreatePlayerHandlerFunctionType AsyncloginpbLoginServiceCreatePlayerHandler;



class ::loginpb::CreatePlayerC2LRequest;

void SendloginpbLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerC2LRequest& request);
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncloginpbLoginServiceEnterGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::loginpb::EnterGameC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::loginpb::EnterGameC2LResponse>> response_reader;
};

using AsyncloginpbLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceEnterGameGrpcClientCall>&)>;
extern AsyncloginpbLoginServiceEnterGameHandlerFunctionType AsyncloginpbLoginServiceEnterGameHandler;



class ::loginpb::EnterGameC2LRequest;

void SendloginpbLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameC2LRequest& request);
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncloginpbLoginServiceLeaveGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncloginpbLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceLeaveGameGrpcClientCall>&)>;
extern AsyncloginpbLoginServiceLeaveGameHandlerFunctionType AsyncloginpbLoginServiceLeaveGameHandler;



class ::loginpb::LeaveGameC2LRequest;

void SendloginpbLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameC2LRequest& request);
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncloginpbLoginServiceDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncloginpbLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncloginpbLoginServiceDisconnectGrpcClientCall>&)>;
extern AsyncloginpbLoginServiceDisconnectHandlerFunctionType AsyncloginpbLoginServiceDisconnectHandler;



class ::loginpb::LoginNodeDisconnectRequest;

void SendloginpbLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request);
void HandleloginpbLoginServiceCompletedQueueMessage(entt::registry& registry);
void InitloginpbLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

}// namespace loginpb
