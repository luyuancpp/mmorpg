#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/gate_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using GrpcGateServiceStubPtr = std::unique_ptr<::GateService::Stub>;

class AsyncGateServiceRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall>&)>;

extern AsyncGateServiceRegisterGameHandlerFunctionType  AsyncGateServiceRegisterGameHandler;


class ::RegisterGameNodeRequest;
void SendGateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceUnRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceUnRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall>&)>;

extern AsyncGateServiceUnRegisterGameHandlerFunctionType  AsyncGateServiceUnRegisterGameHandler;


class ::UnregisterGameNodeRequest;
void SendGateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::UnregisterGameNodeRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RegisterGameNodeSessionResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::RegisterGameNodeSessionResponse>> response_reader;
};

using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;

extern AsyncGateServicePlayerEnterGameNodeHandlerFunctionType  AsyncGateServicePlayerEnterGameNodeHandler;


class ::RegisterGameNodeSessionRequest;
void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeSessionRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;

extern AsyncGateServiceSendMessageToPlayerHandlerFunctionType  AsyncGateServiceSendMessageToPlayerHandler;


class ::NodeRouteMessageRequest;
void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceKickSessionByCentreGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;

extern AsyncGateServiceKickSessionByCentreHandlerFunctionType  AsyncGateServiceKickSessionByCentreHandler;


class ::KickSessionRequest;
void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const  ::KickSessionRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceRouteNodeMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::RouteMessageResponse>> response_reader;
};

using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;

extern AsyncGateServiceRouteNodeMessageHandlerFunctionType  AsyncGateServiceRouteNodeMessageHandler;


class ::RouteMessageRequest;
void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceRoutePlayerMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
	std::unique_ptr<ClientAsyncResponseReader<::RoutePlayerMessageResponse>> response_reader;
};

using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;

extern AsyncGateServiceRoutePlayerMessageHandlerFunctionType  AsyncGateServiceRoutePlayerMessageHandler;


class ::RoutePlayerMessageRequest;
void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

class AsyncGateServiceBroadcastToPlayersGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
	std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceBroadcastToPlayersHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)>;

extern AsyncGateServiceBroadcastToPlayersHandlerFunctionType  AsyncGateServiceBroadcastToPlayersHandler;


class ::BroadcastToPlayersRequest;
void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const  ::BroadcastToPlayersRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
