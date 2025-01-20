#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/centre_service.grpc.pb.h"
#include "proto/common/centre_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcCentreServiceStubPtr = std::unique_ptr<CentreService::Stub>;
class AsyncCentreServiceRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGameNodeRequest;
void CentreServiceRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGameNodeRequest& request);
class AsyncCentreServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGateNodeRequest;
void CentreServiceRegisterGateNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGateNodeRequest& request);
class AsyncCentreServiceGatePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateClientMessageRequest;
void CentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, const GateClientMessageRequest& request);
class AsyncCentreServiceGateSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const GateSessionDisconnectRequest& request);
class AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoginResponse>> response_reader;
};

class LoginRequest;
void CentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, const LoginRequest& request);
class AsyncCentreServiceLoginNodeEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class CentrePlayerGameNodeEntryRequest;
void CentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, const CentrePlayerGameNodeEntryRequest& request);
class AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class LoginNodeLeaveGameRequest;
void CentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, const LoginNodeLeaveGameRequest& request);
class AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const GateSessionDisconnectRequest& request);
class AsyncCentreServicePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void CentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request);
class AsyncCentreServiceEnterGsSucceedGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class EnterGameNodeSuccessRequest;
void CentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, const EnterGameNodeSuccessRequest& request);
class AsyncCentreServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void CentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const RouteMessageRequest& request);
class AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void CentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const RoutePlayerMessageRequest& request);
class AsyncCentreServiceUnRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void CentreServiceUnRegisterGameNode(entt::registry& registry, entt::entity nodeEntity, const UnregisterGameNodeRequest& request);

void HandleCentreServiceCompletedQueueMessage(entt::registry& registry	); 

void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);
