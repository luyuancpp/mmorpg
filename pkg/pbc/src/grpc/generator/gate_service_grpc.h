#pragma once

#include "proto/common/gate_service.grpc.pb.h"
#include "proto/common/gate_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
class AsyncGateServiceRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGameNodeRequest;
void GateServiceRegisterGame(const RegisterGameNodeRequest& request);
class AsyncGateServiceUnRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void GateServiceUnRegisterGame(const UnregisterGameNodeRequest& request);
class AsyncGateServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RegisterGameNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RegisterGameNodeSessionResponse>> response_reader;
};

class RegisterGameNodeSessionRequest;
void GateServicePlayerEnterGameNode(const RegisterGameNodeSessionRequest& request);
class AsyncGateServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class NodeRouteMessageRequest;
void GateServiceSendMessageToPlayer(const NodeRouteMessageRequest& request);
class AsyncGateServiceKickSessionByCentreGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class KickSessionRequest;
void GateServiceKickSessionByCentre(const KickSessionRequest& request);
class AsyncGateServiceRouteNodeMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void GateServiceRouteNodeMessage(const RouteMessageRequest& request);
class AsyncGateServiceRoutePlayerMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void GateServiceRoutePlayerMessage(const RoutePlayerMessageRequest& request);
class AsyncGateServiceBroadcastToPlayersGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class BroadcastToPlayersRequest;
void GateServiceBroadcastToPlayers(const BroadcastToPlayersRequest& request);

void HandleGateServiceCompletedQueueMessage(); 
