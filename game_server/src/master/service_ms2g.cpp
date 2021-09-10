#include "service_ms2g.h"

#include "src/game_server.h"

#include "src/server_common/closure_auto_done.h"

namespace ms2g
{
    void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::ms2g::EnterGameRequest* request,
        ::ms2g::EnterGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
    }

}