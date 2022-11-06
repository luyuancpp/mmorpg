#include "database_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void DbServiceImpl::Login(::google::protobuf::RpcController* controller,
    const dbservice::LoginRequest* request,
    dbservice::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void DbServiceImpl::CreatePlayer(::google::protobuf::RpcController* controller,
    const dbservice::CreatePlayerRequest* request,
    dbservice::CreatePlayerResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void DbServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const dbservice::EnterGameRequest* request,
    dbservice::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
