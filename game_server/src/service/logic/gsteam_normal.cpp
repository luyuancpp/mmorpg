#include "gsteam_normal.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerTeamServiceImpl::EnterScene(::google::protobuf::RpcController* controller,
    const normal::TeamTestS2SRequest* request,
    normal::TeamTestS2SRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterScene
///<<< END WRITING YOUR CODE EnterScene
}

///<<<rpc end
