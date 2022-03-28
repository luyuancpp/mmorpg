#include "gs2gw.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/server_common/rpc_closure.h"
using namespace common;
///<<< END WRITING YOUR CODE

namespace gs2gw{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void G2rgServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gs2gw::PlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
///<<< END WRITING YOUR CODE PlayerService
}

///<<<rpc end
}// namespace gs2gw
