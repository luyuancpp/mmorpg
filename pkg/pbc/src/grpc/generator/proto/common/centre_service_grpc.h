#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/centre_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using GrpcCentreServiceStubPtr = std::unique_ptr<::CentreService::Stub>;
class AsyncCentreServiceRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::RegisterGameNodeRequest;
void SendCentreServiceRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGameNodeRequest& request);

using AsyncCentreServiceRegisterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGameNodeGrpcClientCall>&)>;

extern AsyncCentreServiceRegisterGameNodeHandlerFunctionType  AsyncCentreServiceRegisterGameNodeHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::RegisterGateNodeRequest;
void SendCentreServiceRegisterGateNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGateNodeRequest& request);

using AsyncCentreServiceRegisterGateNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterGateNodeGrpcClientCall>&)>;

extern AsyncCentreServiceRegisterGateNodeHandlerFunctionType  AsyncCentreServiceRegisterGateNodeHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceGatePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::GateClientMessageRequest;
void SendCentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, const  ::GateClientMessageRequest& request);

using AsyncCentreServiceGatePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall>&)>;

extern AsyncCentreServiceGatePlayerServiceHandlerFunctionType  AsyncCentreServiceGatePlayerServiceHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceGateSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::GateSessionDisconnectRequest;
void SendCentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const  ::GateSessionDisconnectRequest& request);

using AsyncCentreServiceGateSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall>&)>;

extern AsyncCentreServiceGateSessionDisconnectHandlerFunctionType  AsyncCentreServiceGateSessionDisconnectHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::LoginResponse>> response_reader;
};

class ::LoginRequest;
void SendCentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, const  ::LoginRequest& request);

using AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall>&)>;

extern AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType  AsyncCentreServiceLoginNodeAccountLoginHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceLoginNodeEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::CentrePlayerGameNodeEntryRequest;
void SendCentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, const  ::CentrePlayerGameNodeEntryRequest& request);

using AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall>&)>;

extern AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType  AsyncCentreServiceLoginNodeEnterGameHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::LoginNodeLeaveGameRequest;
void SendCentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, const  ::LoginNodeLeaveGameRequest& request);

using AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall>&)>;

extern AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType  AsyncCentreServiceLoginNodeLeaveGameHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::GateSessionDisconnectRequest;
void SendCentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const  ::GateSessionDisconnectRequest& request);

using AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall>&)>;

extern AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType  AsyncCentreServiceLoginNodeSessionDisconnectHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServicePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::NodeRouteMessageResponse>> response_reader;
};

class ::NodeRouteMessageRequest;
void SendCentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

using AsyncCentreServicePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall>&)>;

extern AsyncCentreServicePlayerServiceHandlerFunctionType  AsyncCentreServicePlayerServiceHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceEnterGsSucceedGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::EnterGameNodeSuccessRequest;
void SendCentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, const  ::EnterGameNodeSuccessRequest& request);

using AsyncCentreServiceEnterGsSucceedHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall>&)>;

extern AsyncCentreServiceEnterGsSucceedHandlerFunctionType  AsyncCentreServiceEnterGsSucceedHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RouteMessageResponse>> response_reader;
};

class ::RouteMessageRequest;
void SendCentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request);

using AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall>&)>;

extern AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType  AsyncCentreServiceRouteNodeStringMsgHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RoutePlayerMessageResponse>> response_reader;
};

class ::RoutePlayerMessageRequest;
void SendCentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request);

using AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall>&)>;

extern AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType  AsyncCentreServiceRoutePlayerStringMsgHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
class AsyncCentreServiceUnRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::UnregisterGameNodeRequest;
void SendCentreServiceUnRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::UnregisterGameNodeRequest& request);

using AsyncCentreServiceUnRegisterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceUnRegisterGameNodeGrpcClientCall>&)>;

extern AsyncCentreServiceUnRegisterGameNodeHandlerFunctionType  AsyncCentreServiceUnRegisterGameNodeHandler;;

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
