#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/gate_service.grpc.pb.h"


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
void GateServiceRegisterGame(entt::registry& registry, entt::entity nodeEntity, const RegisterGameNodeRequest& request);
class AsyncGateServiceUnRegisterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void GateServiceUnRegisterGame(entt::registry& registry, entt::entity nodeEntity, const UnregisterGameNodeRequest& request);
class AsyncGateServicePlayerEnterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RegisterGameNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RegisterGameNodeSessionResponse>> response_reader;
};

class RegisterGameNodeSessionRequest;
void GateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGameNodeSessionRequest& request);
class AsyncGateServiceSendMessageToPlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class NodeRouteMessageRequest;
void GateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request);
class AsyncGateServiceKickSessionByCentreGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class KickSessionRequest;
void GateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const KickSessionRequest& request);
class AsyncGateServiceRouteNodeMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void GateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const RouteMessageRequest& request);
class AsyncGateServiceRoutePlayerMessageGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void GateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const RoutePlayerMessageRequest& request);
class AsyncGateServiceBroadcastToPlayersGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class BroadcastToPlayersRequest;
void GateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const BroadcastToPlayersRequest& request);

void HandleGateServiceCompletedQueueMessage(entt::registry& registry	); 

void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
