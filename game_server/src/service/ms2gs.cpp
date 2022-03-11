#include "ms2gs.h"

///<<< BEGIN WRITING YOUR CODE 
#include "muduo/base/Logging.h"

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
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
    g_players.emplace(request->player_id(), common::EntityPtr());
///<<< END WRITING YOUR CODE EnterGame
}

///<<<rpc end
}// namespace ms2gs
