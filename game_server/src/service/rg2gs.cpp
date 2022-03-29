#include "rg2gs.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

using namespace common;
namespace rg2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void Rg2gServiceImpl::EnterRoom(::google::protobuf::RpcController* controller,
    const rg2gs::Rg2gTest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterRoom
///<<< END WRITING YOUR CODE EnterRoom
}

///<<<rpc end
}// namespace rg2gs
