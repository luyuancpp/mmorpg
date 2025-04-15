#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/login_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using GrpcLoginServiceStubPtr = std::unique_ptr<::LoginService::Stub>;

class AsyncLoginServiceLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::LoginC2LResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::LoginC2LResponse>> response_reader;
};

using AsyncLoginServiceLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;

extern AsyncLoginServiceLoginHandlerFunctionType  AsyncLoginServiceLoginHandler;


class ::LoginC2LRequest;
void SendLoginServiceLogin(entt::registry& registry, entt::entity nodeEntity, const  ::LoginC2LRequest& request);

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncLoginServiceCreatePlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::CreatePlayerC2LResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::CreatePlayerC2LResponse>> response_reader;
};

using AsyncLoginServiceCreatePlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;

extern AsyncLoginServiceCreatePlayerHandlerFunctionType  AsyncLoginServiceCreatePlayerHandler;


class ::CreatePlayerC2LRequest;
void SendLoginServiceCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const  ::CreatePlayerC2LRequest& request);

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncLoginServiceEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::EnterGameC2LResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::EnterGameC2LResponse>> response_reader;
};

using AsyncLoginServiceEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;

extern AsyncLoginServiceEnterGameHandlerFunctionType  AsyncLoginServiceEnterGameHandler;


class ::EnterGameC2LRequest;
void SendLoginServiceEnterGame(entt::registry& registry, entt::entity nodeEntity, const  ::EnterGameC2LRequest& request);

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncLoginServiceLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncLoginServiceLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;

extern AsyncLoginServiceLeaveGameHandlerFunctionType  AsyncLoginServiceLeaveGameHandler;


class ::LeaveGameC2LRequest;
void SendLoginServiceLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  ::LeaveGameC2LRequest& request);

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncLoginServiceDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncLoginServiceDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncLoginServiceDisconnectGrpcClientCall>&)>;

extern AsyncLoginServiceDisconnectHandlerFunctionType  AsyncLoginServiceDisconnectHandler;


class ::LoginNodeDisconnectRequest;
void SendLoginServiceDisconnect(entt::registry& registry, entt::entity nodeEntity, const  ::LoginNodeDisconnectRequest& request);

void HandleLoginServiceCompletedQueueMessage(entt::registry& registry	); 
void InitLoginServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
