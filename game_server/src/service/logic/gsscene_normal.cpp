#include "gsscene_normal.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

using namespace common;
namespace normal{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerSceneServiceImpl::UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
    const normal::UpdateCrossMainSceneInfoRequest* request,
    normal::UpdateCrossMainSceneInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE UpdateCrossMainSceneInfo
///<<< END WRITING YOUR CODE UpdateCrossMainSceneInfo
}

///<<<rpc end
}// namespace normal
