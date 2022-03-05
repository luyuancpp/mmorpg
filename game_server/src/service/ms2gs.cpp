#include "ms2gs.h"

///<<< BEGIN WRITING YOUR CODE 
#include "muduo/base/Logging.h"

#include "src/game_server.h"
#include "src/server_common/closure_auto_done.h"

using namespace common;
///<<< END WRITING YOUR CODE

namespace ms2g{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE

/// ///<<<rpc begin
void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ms2g::EnterGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
    //LOG_INFO << request->DebugString().c_str();
///<<< END WRITING YOUR CODE EnterGame
}

///<<<rpc end
}// namespace ms2g
