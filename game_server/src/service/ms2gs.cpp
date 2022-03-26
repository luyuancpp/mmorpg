#include "ms2gs.h"

///<<< BEGIN WRITING YOUR CODE 
#include "muduo/base/Logging.h"

#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_server.h"
#include "src/module/player_list/player_list.h"
#include "src/server_common/closure_auto_done.h"
using namespace common;
using namespace game;
///<<< END WRITING YOUR CODE

namespace ms2gs{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE

/// ///<<<rpc begin
void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ms2gs::EnterGameRequest* request,
    ms2gs::EnterGameRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
    auto it =  g_players.emplace(request->player_id(), common::EntityPtr());
    if (it.second)
    {
        reg.emplace<GateConnId>(it.first->second.entity(), request->conn_id());
    }
///<<< END WRITING YOUR CODE EnterGame
}

void Ms2gServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const ms2gs::Ms2GsPlayerMessageRequest* request,
    ms2gs::Ms2GsPlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
///<<< END WRITING YOUR CODE PlayerService
}

void Ms2gServiceImpl::PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
    const ms2gs::Ms2GsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerServiceNoRespone
///<<< END WRITING YOUR CODE PlayerServiceNoRespone
}

///<<<rpc end
}// namespace ms2gs
