#include "scene_client_player.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/game_registry.h"
#include "src/module/network/message_sys.h"

#include "logic_proto/scene_server_player.pb.h"
using namespace serverplayer;
using namespace common;
///<<< END WRITING YOUR CODE

namespace clientplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void PlayerPlayerSceneServiceImpl::EnterScene(common::EntityPtr& entity,
    const clientplayer::SeceneTestRequest* request,
    clientplayer::SeceneTestResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterScene
    Ms2GsLoginRequest msg;
    Send2MsPlayer(msg, reg.get<common::Guid>(entity.entity()));
///<<< END WRITING YOUR CODE EnterScene
}

void PlayerPlayerSceneServiceImpl::EnterSceneNotify(common::EntityPtr& entity,
    const clientplayer::EnterSeceneS2C* request,
    clientplayer::EnterSeceneS2C* response)
{
///<<< BEGIN WRITING YOUR CODE EnterSceneNotify
///<<< END WRITING YOUR CODE EnterSceneNotify
}

///<<<rpc end
///<<< BEGIN WRITING YOUR CODE EnterSceneNotify
///<<< END WRITING YOUR CODE EnterSceneNotify

}// namespace clientplayer
