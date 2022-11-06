#include "game_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void GsServiceImpl::EnterGs(::google::protobuf::RpcController* controller,
    const gsservice::EnterGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gsservice::ControllerPlayerMessageRequest* request,
    gsservice::ControllerPlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::GatePlayerService(::google::protobuf::RpcController* controller,
    const gsservice::RpcClientRequest* request,
    gsservice::RpcClientResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gsservice::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::GateConnectGs(::google::protobuf::RpcController* controller,
    const gsservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::ControllerSend2PlayerViaGs(::google::protobuf::RpcController* controller,
    const gsservice::ControllerSend2PlayerViaGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
