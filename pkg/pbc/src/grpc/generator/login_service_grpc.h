#pragma once

#include "proto/common/login_service.grpc.pb.h"
#include "proto/common/login_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcLoginServiceStubPtr = std::unique_ptr<LoginService::Stub>;
class AsyncLoginServiceLoginGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoginC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoginC2LResponse>> response_reader;
};

class LoginC2LRequest;
void LoginServiceLogin(GrpcLoginServiceStubPtr& stub, const LoginC2LRequest& request);
class AsyncLoginServiceCreatePlayerGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    CreatePlayerC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< CreatePlayerC2LResponse>> response_reader;
};

class CreatePlayerC2LRequest;
void LoginServiceCreatePlayer(GrpcLoginServiceStubPtr& stub, const CreatePlayerC2LRequest& request);
class AsyncLoginServiceEnterGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    EnterGameC2LResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< EnterGameC2LResponse>> response_reader;
};

class EnterGameC2LRequest;
void LoginServiceEnterGame(GrpcLoginServiceStubPtr& stub, const EnterGameC2LRequest& request);
class AsyncLoginServiceLeaveGameGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class LeaveGameC2LRequest;
void LoginServiceLeaveGame(GrpcLoginServiceStubPtr& stub, const LeaveGameC2LRequest& request);
class AsyncLoginServiceDisconnectGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    Empty reply;
    std::unique_ptr<ClientAsyncResponseReader< Empty>> response_reader;
};

class LoginNodeDisconnectRequest;
void LoginServiceDisconnect(GrpcLoginServiceStubPtr& stub, const LoginNodeDisconnectRequest& request);

void HandleLoginServiceCompletedQueueMessage(); 
