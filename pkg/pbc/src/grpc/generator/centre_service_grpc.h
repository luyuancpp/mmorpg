#pragma once

#include "proto/common/centre_service.grpc.pb.h"
#include "proto/common/centre_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
class AsyncCentreServiceRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGameNodeRequest;
void CentreServiceRegisterGameNode(const RegisterGameNodeRequest& request);
class AsyncCentreServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGateNodeRequest;
void CentreServiceRegisterGateNode(const RegisterGateNodeRequest& request);
class AsyncCentreServiceGatePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateClientMessageRequest;
void CentreServiceGatePlayerService(const GateClientMessageRequest& request);
class AsyncCentreServiceGateSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceGateSessionDisconnect(const GateSessionDisconnectRequest& request);
class AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoginResponse>> response_reader;
};

class LoginRequest;
void CentreServiceLoginNodeAccountLogin(const LoginRequest& request);
class AsyncCentreServiceLoginNodeEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class CentrePlayerGameNodeEntryRequest;
void CentreServiceLoginNodeEnterGame(const CentrePlayerGameNodeEntryRequest& request);
class AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class LoginNodeLeaveGameRequest;
void CentreServiceLoginNodeLeaveGame(const LoginNodeLeaveGameRequest& request);
class AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceLoginNodeSessionDisconnect(const GateSessionDisconnectRequest& request);
class AsyncCentreServicePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void CentreServicePlayerService(const NodeRouteMessageRequest& request);
class AsyncCentreServiceEnterGsSucceedGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class EnterGameNodeSuccessRequest;
void CentreServiceEnterGsSucceed(const EnterGameNodeSuccessRequest& request);
class AsyncCentreServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void CentreServiceRouteNodeStringMsg(const RouteMessageRequest& request);
class AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void CentreServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request);
class AsyncCentreServiceUnRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void CentreServiceUnRegisterGameNode(const UnregisterGameNodeRequest& request);

void HandleCentreServiceCompletedQueueMessage(); 
