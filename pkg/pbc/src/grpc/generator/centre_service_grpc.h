#pragma once

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
void CentreServiceRegisterGameNode(GrpcCentreServiceStubPtr& stub, const RegisterGameNodeRequest& request);
class AsyncCentreServiceRegisterGateNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class RegisterGateNodeRequest;
void CentreServiceRegisterGateNode(GrpcCentreServiceStubPtr& stub, const RegisterGateNodeRequest& request);
class AsyncCentreServiceGatePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateClientMessageRequest;
void CentreServiceGatePlayerService(GrpcCentreServiceStubPtr& stub, const GateClientMessageRequest& request);
class AsyncCentreServiceGateSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceGateSessionDisconnect(GrpcCentreServiceStubPtr& stub, const GateSessionDisconnectRequest& request);
class AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoginResponse>> response_reader;
};

class LoginRequest;
void CentreServiceLoginNodeAccountLogin(GrpcCentreServiceStubPtr& stub, const LoginRequest& request);
class AsyncCentreServiceLoginNodeEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class CentrePlayerGameNodeEntryRequest;
void CentreServiceLoginNodeEnterGame(GrpcCentreServiceStubPtr& stub, const CentrePlayerGameNodeEntryRequest& request);
class AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class LoginNodeLeaveGameRequest;
void CentreServiceLoginNodeLeaveGame(GrpcCentreServiceStubPtr& stub, const LoginNodeLeaveGameRequest& request);
class AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class GateSessionDisconnectRequest;
void CentreServiceLoginNodeSessionDisconnect(GrpcCentreServiceStubPtr& stub, const GateSessionDisconnectRequest& request);
class AsyncCentreServicePlayerServiceGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeRouteMessageResponse>> response_reader;
};

class NodeRouteMessageRequest;
void CentreServicePlayerService(GrpcCentreServiceStubPtr& stub, const NodeRouteMessageRequest& request);
class AsyncCentreServiceEnterGsSucceedGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class EnterGameNodeSuccessRequest;
void CentreServiceEnterGsSucceed(GrpcCentreServiceStubPtr& stub, const EnterGameNodeSuccessRequest& request);
class AsyncCentreServiceRouteNodeStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RouteMessageResponse>> response_reader;
};

class RouteMessageRequest;
void CentreServiceRouteNodeStringMsg(GrpcCentreServiceStubPtr& stub, const RouteMessageRequest& request);
class AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RoutePlayerMessageResponse>> response_reader;
};

class RoutePlayerMessageRequest;
void CentreServiceRoutePlayerStringMsg(GrpcCentreServiceStubPtr& stub, const RoutePlayerMessageRequest& request);
class AsyncCentreServiceUnRegisterGameNodeGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class UnregisterGameNodeRequest;
void CentreServiceUnRegisterGameNode(GrpcCentreServiceStubPtr& stub, const UnregisterGameNodeRequest& request);

void HandleCentreServiceCompletedQueueMessage(); 
