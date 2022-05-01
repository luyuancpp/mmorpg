#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "logic_proto/scene_server_player.pb.h"
using namespace serverplayer;
using namespace common;
///<<< END WRITING YOUR CODE

namespace clientplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void PlayerSceneServiceImpl::EnterScene(common::EntityPtr& entity,
    const clientplayer::SeceneTestRequest* request,
    clientplayer::SeceneTestResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterScene
    Gs2MsLoginRequest msg;
	Send2MsPlayer(msg, reg.get<Guid>(entity.entity()));
///<<< END WRITING YOUR CODE EnterScene
}

void PlayerSceneServiceImpl::EnterSceneNotify(common::EntityPtr& entity,
    const clientplayer::EnterSeceneS2C* request,
    clientplayer::EnterSeceneS2C* response)
{
///<<< BEGIN WRITING YOUR CODE EnterSceneNotify
///<<< END WRITING YOUR CODE EnterSceneNotify
}

///<<<rpc end
}// namespace clientplayer
