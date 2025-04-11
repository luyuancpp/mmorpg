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
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::RegisterGameNodeRequest;
void SendGateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeRequest& request);

using AsyncGateServiceRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterGameGrpcClientCall>&)>;

extern AsyncGateServiceRegisterGameHandlerFunctionType  AsyncGateServiceRegisterGameHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServiceUnRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::UnregisterGameNodeRequest;
void SendGateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, const  ::UnregisterGameNodeRequest& request);

using AsyncGateServiceUnRegisterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceUnRegisterGameGrpcClientCall>&)>;

extern AsyncGateServiceUnRegisterGameHandlerFunctionType  AsyncGateServiceUnRegisterGameHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RegisterGameNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RegisterGameNodeSessionResponse>> response_reader;
};

class ::RegisterGameNodeSessionRequest;
void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeSessionRequest& request);

using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;

extern AsyncGateServicePlayerEnterGameNodeHandlerFunctionType  AsyncGateServicePlayerEnterGameNodeHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::NodeRouteMessageRequest;
void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;

extern AsyncGateServiceSendMessageToPlayerHandlerFunctionType  AsyncGateServiceSendMessageToPlayerHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServiceKickSessionByCentreGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::KickSessionRequest;
void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const  ::KickSessionRequest& request);

using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;

extern AsyncGateServiceKickSessionByCentreHandlerFunctionType  AsyncGateServiceKickSessionByCentreHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServiceRouteNodeMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RouteMessageResponse>> response_reader;
};

class ::RouteMessageRequest;
void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request);

using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;

extern AsyncGateServiceRouteNodeMessageHandlerFunctionType  AsyncGateServiceRouteNodeMessageHandler;;

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncGateServiceRoutePlayerMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RoutePlayerMessageResponse>> response_reader;
};

class ::RoutePlayerMessageRequest;
void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request);

using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;

extern AsyncGateService