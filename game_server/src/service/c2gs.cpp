#include "c2gs.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/server_common/closure_auto_done.h"

using namespace  common;
///<<< END WRITING YOUR CODE

namespace c2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void C2GsServiceImpl::EnterScene(::google::protobuf::RpcController* controller,
    const c2gs::EnterSceneRequest* request,
    c2gs::EnterSceneResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterScene
    LOG_INFO << "enter gs "; 
///<<< END WRITING YOUR CODE EnterScene
}

///<<<rpc end
}// namespace c2gs
