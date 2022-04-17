#include "rg_node.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

using namespace common;
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void G2rgServiceImpl::StartCrossMainGS(::google::protobuf::RpcController* controller,
    const regionservcie::StartGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossMainGS
///<<< END WRITING YOUR CODE StartCrossMainGS
}

void G2rgServiceImpl::StartCrossRoomGS(::google::protobuf::RpcController* controller,
    const regionservcie::StartGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossRoomGS
///<<< END WRITING YOUR CODE StartCrossRoomGS
}

///<<<rpc end
}// namespace regionservcie
