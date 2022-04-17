#include "rg_node.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

using namespace common;
namespace rgservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossMainGS(::google::protobuf::RpcController* controller,
    const rgservcie::StartMainRoomGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossMainGS
///<<< END WRITING YOUR CODE StartCrossMainGS
}

void RgServiceImpl::StartCrossRoomGS(::google::protobuf::RpcController* controller,
    const rgservcie::StartCrossRoomGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossRoomGS
///<<< END WRITING YOUR CODE StartCrossRoomGS
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
