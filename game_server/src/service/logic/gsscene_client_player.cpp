#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ClientPlayerSceneServiceImpl::EnterSceneC2S(EntityPtr& entity,
    const ::EnterSeceneC2SRequest* request,
    ::EnterSeceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
    //如果是跨服副本服不能换场景

    Send2MsPlayer();
///<<< END WRITING YOUR CODE
}

///<<<rpc end
