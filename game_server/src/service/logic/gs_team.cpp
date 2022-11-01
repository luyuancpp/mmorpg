#include "gs_team.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void NormalS2STeamServiceImpl::EnterScene(::google::protobuf::RpcController* controller,
    const ::TeamTestS2SRequest* request,
    ::TeamTestS2SRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
