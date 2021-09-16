#include "service_ms2g.h"

#include "muduo/base/Logging.h"

#include "src/game_server.h"
#include "src/server_common/closure_auto_done.h"

namespace ms2g
{
    void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::ms2g::EnterGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
        LOG_INFO << request->DebugString().c_str();
    }

}