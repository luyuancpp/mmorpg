#include "rg2gs.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/server_common/closure_auto_done.h"

using namespace common;
///<<< END WRITING YOUR CODE

namespace rg2g{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void Rg2gServiceImpl::EnterRoom(::google::protobuf::RpcController* controller,
    const rg2g::Rg2gTest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterRoom
///<<< END WRITING YOUR CODE EnterRoom
}

///<<<rpc end
}// namespace rg2g
