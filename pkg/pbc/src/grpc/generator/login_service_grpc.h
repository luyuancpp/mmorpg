#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/login_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcLoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;
class AsyncLoginServiceLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    LoginC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  LoginC2LResponse>> response_reader;
};

class LoginC2LRequest;
void LoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const  LoginC2LRequest& request);

using AsyncLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;

extern AsyncLoginServiceLoginHandlerFunctionType  AsyncLoginServiceLoginHandler;;
class AsyncLoginServiceCreatePlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    CreatePlayerC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  CreatePlayerC2LResponse>> response_reader;
};

class CreatePlayerC2LRequest;
void LoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const  CreatePlayerC2LRequest& request);

using AsyncLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;

extern AsyncLoginServiceCreatePlayerHandlerFunctionType  AsyncLoginServiceCreatePlayerHandler;;
class AsyncLoginServiceEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    EnterGameC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  EnterGameC2LResponse>> response_reader;
};

class EnterGameC2LRequest;
void LoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const  EnterGameC2LRequest& request);

using AsyncLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;

extern AsyncLoginServiceEnterGameHandlerFunctionType  AsyncLoginServiceEnterGameHandler;;
class AsyncLoginServiceLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  Empty>> response_reader;
};

class LeaveGameC2LRequest;
void LoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  LeaveGameC2LRequest& request);

using AsyncLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;

extern AsyncLoginServiceLeaveGameHandlerFunctionType  AsyncLoginServiceLeaveGameHandler;;
class AsyncLoginServiceDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  Empty>> response_reader;
};

class LoginNodeDisconnectRequest;
void LoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const  LoginNodeDisconnectRequest& request);

using AsyncLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)>;

extern AsyncLoginServiceDisconnectHandlerFunctionType  AsyncLoginServiceDisconnectHandler;;

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 

void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


