#include "gsscene_normal.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerSceneServiceImpl::UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
    const ::UpdateCrossMainSceneInfoRequest* request,
    ::UpdateCrossMainSceneInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE UpdateCrossMainSceneInfo
///<<< END WRITING YOUR CODE UpdateCrossMainSceneInfo
}

void ServerSceneServiceImpl::Ms2GsEnterScene(::google::protobuf::RpcController* controller,
    const ::Ms2GsEnterSceneRequest* request,
    ::Ms2GsEnterSceneRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Ms2GsEnterScene
    //顶号，或者登录走到这里

///<<< END WRITING YOUR CODE Ms2GsEnterScene
}

void ServerSceneServiceImpl::Ms2GsReconnectEnterScene(::google::protobuf::RpcController* controller,
    const ::Ms2GsEnterSceneRequest* request,
    ::Ms2GsEnterSceneRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Ms2GsReconnectEnterScene
///<<< END WRITING YOUR CODE Ms2GsReconnectEnterScene
}

///<<<rpc end
