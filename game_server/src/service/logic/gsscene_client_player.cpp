#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ClientPlayerSceneServiceImpl::EnterSceneC2S(entt::entity& player,
    const ::EnterSeceneC2SRequest* request,
    ::EnterSeceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
    //如果是跨服副本服不能换场景
    Gs2MsEnterSceneRequest message;
    Send2MsPlayer(message, player);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
