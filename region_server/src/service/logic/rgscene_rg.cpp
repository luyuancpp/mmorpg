#include "rgscene_rg.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
///<<< END WRITING YOUR CODE

using namespace common;
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossMainGS(::google::protobuf::RpcController* controller,
    const regionservcie::StartCrossMainGSRequest* request,
    regionservcie::StartCrossMainGSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossMainGS
///<<< END WRITING YOUR CODE StartCrossMainGS
}

void RgServiceImpl::StartCrossRoomGS(::google::protobuf::RpcController* controller,
    const regionservcie::StartCrossRoomGSRequest* request,
    regionservcie::StartCrossRoomGSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossRoomGS
    
///<<< END WRITING YOUR CODE StartCrossRoomGS
}

void RgServiceImpl::StartMS(::google::protobuf::RpcController* controller,
    const regionservcie::StartMSRequest* request,
    regionservcie::StartMSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartMS
    //LOG_INFO << request->ms_node_id();
///<<< END WRITING YOUR CODE StartMS
}

///<<<rpc end
}// namespace regionservcie
