#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/game_service.grpc.pb.h"
#include "proto/common/game_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcGameServiceStubPtr = std::unique_ptr<GameService::Stub>;
class AsyncGameServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class PlayerEnterGameNodeRequest;
void GameServicePlayerEnterGameNode(GrpcGameServiceStubPtr& stub, const PlayerEnterGameNodeRequest& request);
class AsyncGameServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceSendMessageToPlayer(GrpcGameServiceStubPtr& stub, const NodeRouteMessageRequest& request);
class AsyncGameServiceClientSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    ClientSendMessageToPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< ClientSendMessageToPlayerResponse>> response_reader;
};

class ClientSendMessageToPlayerRequest;
void GameServiceClientSendMessageToPlayer(GrpcGameServiceStubPtr& stub, const ClientSendMessageToPlayerRequest& request);
class AsyncGameServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGateNodeRequest;
void GameServiceRegisterGateNode(GrpcGameServiceStubPtr& stub, const RegisterGateNodeRequest& request);
class AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceCentreSendToPlayerViaGameNode(GrpcGameServiceStubPtr& stub, const NodeRouteMessageRequest& request);
class AsyncGameServiceInvokePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceInvokePlayerService(GrpcGameServiceStubPtr& stub, const NodeRouteMessageRequest& request);
class AsyncGameServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void GameServiceRouteNodeStringMsg(GrpcGameServiceStubPtr& stub, const RouteMessageRequest& request);
class AsyncGameServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void GameServiceRoutePlayerStringMsg(GrpcGameServiceStubPtr& stub, const RoutePlayerMessageRequest& request);
class AsyncGameServiceUpdateSessionDetailGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterPlayerSessionRequest;
void GameServiceUpdateSessionDetail(GrpcGameServiceStubPtr& stub, const RegisterPlayerSessionRequest& request);
class AsyncGameServiceEnterSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class Centre2GsEnterSceneRequest;
void GameServiceEnterScene(GrpcGameServiceStubPtr& stub, const Centre2GsEnterSceneRequest& request);
class AsyncGameServiceCreateSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< CreateSceneResponse>> response_reader;
};

class CreateSceneRequest;
void GameServiceCreateScene(GrpcGameServiceStubPtr& stub, const CreateSceneRequest& request);

void HandleGameServiceCompletedQueueMessage(entt::registry& registry	); 

void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
