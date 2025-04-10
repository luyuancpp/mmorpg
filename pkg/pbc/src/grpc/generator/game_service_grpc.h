#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/game_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcGameServiceStubPtr = std::unique_ptr<::GameService::Stub>;
class AsyncGameServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::PlayerEnterGameNodeRequest;
void SendGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::PlayerEnterGameNodeRequest& request);

using AsyncGameServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall>&)>;

extern AsyncGameServicePlayerEnterGameNodeHandlerFunctionType  AsyncGameServicePlayerEnterGameNodeHandler;;
class AsyncGameServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::NodeRouteMessageResponse>> response_reader;
};

class ::NodeRouteMessageRequest;
void SendGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

using AsyncGameServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall>&)>;

extern AsyncGameServiceSendMessageToPlayerHandlerFunctionType  AsyncGameServiceSendMessageToPlayerHandler;;
class AsyncGameServiceClientSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::ClientSendMessageToPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::ClientSendMessageToPlayerResponse>> response_reader;
};

class ::ClientSendMessageToPlayerRequest;
void SendGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const  ::ClientSendMessageToPlayerRequest& request);

using AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall>&)>;

extern AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType  AsyncGameServiceClientSendMessageToPlayerHandler;;
class AsyncGameServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::RegisterGateNodeRequest;
void SendGameServiceRegisterGateNode(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterGateNodeRequest& request);

using AsyncGameServiceRegisterGateNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRegisterGateNodeGrpcClientCall>&)>;

extern AsyncGameServiceRegisterGateNodeHandlerFunctionType  AsyncGameServiceRegisterGateNodeHandler;;
class AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::NodeRouteMessageRequest;
void SendGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

using AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall>&)>;

extern AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType  AsyncGameServiceCentreSendToPlayerViaGameNodeHandler;;
class AsyncGameServiceInvokePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::NodeRouteMessageResponse>> response_reader;
};

class ::NodeRouteMessageRequest;
void SendGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const  ::NodeRouteMessageRequest& request);

using AsyncGameServiceInvokePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall>&)>;

extern AsyncGameServiceInvokePlayerServiceHandlerFunctionType  AsyncGameServiceInvokePlayerServiceHandler;;
class AsyncGameServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RouteMessageResponse>> response_reader;
};

class ::RouteMessageRequest;
void SendGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const  ::RouteMessageRequest& request);

using AsyncGameServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall>&)>;

extern AsyncGameServiceRouteNodeStringMsgHandlerFunctionType  AsyncGameServiceRouteNodeStringMsgHandler;;
class AsyncGameServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RoutePlayerMessageResponse>> response_reader;
};

class ::RoutePlayerMessageRequest;
void SendGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const  ::RoutePlayerMessageRequest& request);

using AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall>&)>;

extern AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType  AsyncGameServiceRoutePlayerStringMsgHandler;;
class AsyncGameServiceUpdateSessionDetailGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::RegisterPlayerSessionRequest;
void SendGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const  ::RegisterPlayerSessionRequest& request);

using AsyncGameServiceUpdateSessionDetailHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall>&)>;

extern AsyncGameServiceUpdateSessionDetailHandlerFunctionType  AsyncGameServiceUpdateSessionDetailHandler;;
class AsyncGameServiceEnterSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::Empty>> response_reader;
};

class ::Centre2GsEnterSceneRequest;
void SendGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const  ::Centre2GsEnterSceneRequest& request);

using AsyncGameServiceEnterSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall>&)>;

extern AsyncGameServiceEnterSceneHandlerFunctionType  AsyncGameServiceEnterSceneHandler;;
class AsyncGameServiceCreateSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::CreateSceneResponse>> response_reader;
};

class ::CreateSceneRequest;
void SendGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const  ::CreateSceneRequest& request);

using AsyncGameServiceCreateSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall>&)>;

extern AsyncGameServiceCreateSceneHandlerFunctionType  AsyncGameServiceCreateSceneHandler;;

void HandleGameServiceCompletedQueueMessage(entt::registry& registry	); 

void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


