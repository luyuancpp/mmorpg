#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ClientPlayerSceneServiceImpl::EnterScene(EntityPtr& entity,
    const ::EnterSeceneC2SRequest* request,
    ::EnterSeceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterScene
    Gs2MsLoginRequest msg;
	Send2MsPlayer(msg, reg.get<Guid>(entity.entity()));
///<<< END WRITING YOUR CODE EnterScene
}

void ClientPlayerSceneServiceImpl::EnterSceneNotify(EntityPtr& entity,
    const ::EnterSeceneS2C* request,
    ::EnterSeceneS2C* response)
{
///<<< BEGIN WRITING YOUR CODE EnterSceneNotify
///<<< END WRITING YOUR CODE EnterSceneNotify
}

///<<<rpc end
