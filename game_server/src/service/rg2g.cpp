#include "rg2g.h"

#include "src/game_server.h"

#include "src/server_common/closure_auto_done.h"

using namespace common;

namespace rg2g
{
void Rg2gServiceImpl::EnterRoom(::google::protobuf::RpcController* controller,
    const ::google::protobuf::Empty* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure cp(done);
}
}//namespace rg2g


