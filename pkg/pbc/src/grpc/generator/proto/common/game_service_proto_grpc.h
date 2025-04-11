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

void HandleGameServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
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

void HandleGameServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
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

void HandleGameServiceCompletedQueueMessage(entt::registry& registry	); 
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
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

extern AsyncGameServiceRegisterGateNodeHandlerFunctionType  AsyncGameService