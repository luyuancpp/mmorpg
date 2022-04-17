#include "rg_node.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
///<<< END WRITING YOUR CODE

using namespace common;
namespace rgservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossGS(::google::protobuf::RpcController* controller,
    const rgservcie::StartMainRoomGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossGS
    
///<<< END WRITING YOUR CODE StartCrossGS
}

void RgServiceImpl::StartMs(::google::protobuf::RpcController* controller,
    const rgservcie::StartMsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartMs
    
///<<< END WRITING YOUR CODE StartMs
}

///<<<rpc end
}// namespace rgservcie
