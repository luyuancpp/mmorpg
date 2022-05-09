#include "msscene_normal.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerSceneServiceImpl::UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
    const ::UpdateCrossMainSceneInfoRequest* request,
    ::UpdateCrossMainSceneInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void ServerSceneServiceImpl::Ms2GsEnterScene(::google::protobuf::RpcController* controller,
    const ::Ms2GsEnterSceneRequest* request,
    ::Ms2GsEnterSceneRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
    
///<<< END WRITING YOUR CODE 
}

void ServerSceneServiceImpl::Ms2GsReconnectEnterScene(::google::protobuf::RpcController* controller,
    const ::Ms2GsEnterSceneRequest* request,
    ::Ms2GsEnterSceneRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
