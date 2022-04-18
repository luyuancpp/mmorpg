#include "gsscene_normal.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

using namespace common;
namespace normal{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerSceneServiceImpl::EnterScene(::google::protobuf::RpcController* controller,
    const normal::SceneTestRequest* request,
    normal::SceneTestResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterScene
///<<< END WRITING YOUR CODE EnterScene
}

///<<<rpc end
}// namespace normal
