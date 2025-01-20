#pragma once

#include "proto/common/gate_service.grpc.pb.h"
#include "proto/common/gate_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcGateServiceStubPtr = std::unique_ptr<GateService::Stub>;
class AsyncGateServiceRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGameNodeRequest;
void GateServiceRegisterGame(GrpcGateServiceStubPtr& stub, const RegisterGameNodeRequest& request);
class AsyncGateServiceUnRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void GateServiceUnRegisterGame(GrpcGateServiceStubPtr& stub, const UnregisterGameNodeRequest& request);
class AsyncGateServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RegisterGameNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RegisterGameNodeSessionResponse>> response_reader;
};

class RegisterGameNodeSessionRequest;
void GateServicePlayerEnterGameNode(GrpcGateServiceStubPtr& stub, const RegisterGameNodeSessionRequest& request);
class AsyncGateServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class NodeRouteMessageRequest;
void GateServiceSendMessageToPlayer(GrpcGateServiceStubPtr& stub, const NodeRouteMessageRequest& request);
class AsyncGateServiceKickSessionByCentreGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class KickSessionRequest;
void GateServiceKickSessionByCentre(GrpcGateServiceStubPtr& stub, const KickSessionRequest& request);
class AsyncGateServiceRouteNodeMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void GateServiceRouteNodeMessage(GrpcGateServiceStubPtr& stub, const RouteMessageRequest& request);
class AsyncGateServiceRoutePlayerMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void GateServiceRoutePlayerMessage(GrpcGateServiceStubPtr& stub, const RoutePlayerMessageRequest& request);
class AsyncGateServiceBroadcastToPlayersGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class BroadcastToPlayersRequest;
void GateServiceBroadcastToPlayers(GrpcGateServiceStubPtr& stub, const BroadcastToPlayersRequest& request);

void HandleGateServiceCompletedQueueMessage(); 
