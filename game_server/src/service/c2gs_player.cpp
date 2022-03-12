#include "c2gs_player.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
///<<< END WRITING YOUR CODE

namespace c2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void PlayerC2GsServiceImpl::EnterScene(common::EntityPtr& entity,
    const c2gs::EnterSceneRequest* request,
    c2gs::EnterSceneResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterScene
    LOG_INFO << request->DebugString();
///<<< END WRITING YOUR CODE EnterScene
}

///<<<rpc end
}// namespace c2gs
