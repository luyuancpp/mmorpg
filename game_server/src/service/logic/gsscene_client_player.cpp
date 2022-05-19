#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/network/node_info.h"
#include "src/game_logic/tips_id.h"

#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ClientPlayerSceneServiceImpl::EnterSceneC2S(entt::entity& player,
    const ::EnterSeceneC2SRequest* request,
    ::EnterSeceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
    //如果是跨服副本服不能换场景
    auto server_type = reg.get<GsServerType>(global_entity());
    if (kRoomServer == server_type.server_type_ ||
        kRoomSceneCrossServer == server_type.server_type_)
    {
        response->mutable_error()->set_id(kRetEnterSceneServerType);
        return;
    }
    auto& scene_info = request->scene_info();
    if (scene_info.scene_confid() <= 0 && scene_info.scene_id() <= 0)
    {
        response->mutable_error()->set_id(kRetEnterSceneParamError);
        return;
    }
    Gs2MsEnterSceneRequest message;
    message.mutable_scene_info()->CopyFrom(request->scene_info());
    Send2MsPlayer(message, player);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
