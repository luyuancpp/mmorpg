#include "scene_client_player.h"
///<<< BEGIN WRITING YOUR CODE
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
