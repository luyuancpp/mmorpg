#pragma once

#include "proto/common/game_service.grpc.pb.h"
#include "proto/common/game_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
class AsyncGameServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class PlayerEnterGameNodeRequest;
void GameServicePlayerEnterGameNode(const PlayerEnterGameNodeRequest& request);
class AsyncGameServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceSendMessageToPlayer(const NodeRouteMessageRequest& request);
class AsyncGameServiceClientSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    ClientSendMessageToPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< ClientSendMessageToPlayerResponse>> response_reader;
};

class ClientSendMessageToPlayerRequest;
void GameServiceClientSendMessageToPlayer(const ClientSendMessageToPlayerRequest& request);
class AsyncGameServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGateNodeRequest;
void GameServiceRegisterGateNode(const RegisterGateNodeRequest& request);
class AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceCentreSendToPlayerViaGameNode(const NodeRouteMessageRequest& request);
class AsyncGameServiceInvokePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void GameServiceInvokePlayerService(const NodeRouteMessageRequest& request);
class AsyncGameServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void GameServiceRouteNodeStringMsg(const RouteMessageRequest& request);
class AsyncGameServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void GameServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request);
class AsyncGameServiceUpdateSessionDetailGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterPlayerSessionRequest;
void GameServiceUpdateSessionDetail(const RegisterPlayerSessionRequest& request);
class AsyncGameServiceEnterSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class Centre2GsEnterSceneRequest;
void GameServiceEnterScene(const Centre2GsEnterSceneRequest& request);
class AsyncGameServiceCreateSceneGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< CreateSceneResponse>> response_reader;
};

class CreateSceneRequest;
void GameServiceCreateScene(const CreateSceneRequest& request);