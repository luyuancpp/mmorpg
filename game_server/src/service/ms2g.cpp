#include "ms2g.h"

///<<< BEGIN WRITING YOUR CODE 
#include "muduo/base/Logging.h"

#include "src/game_server.h"
#include "src/server_common/closure_auto_done.h"

using namespace common;
///<<< END WRITING YOUR COD
namespace ms2g
{    
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR COD
//<<<rpc begin
    void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::ms2g::EnterGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
///<<< BEGIN WRITING YOUR CODE 
        AutoRecycleClosure cp(done);
        //LOG_INFO << request->DebugString().c_str();、
///<<< END WRITING YOUR COD
    }
///<<<rpc end
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR COD}// namespace ms2g
}// namespace ms2g
