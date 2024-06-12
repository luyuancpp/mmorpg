#include "scene_client_player_handler.h"
#include "thread_local/thread_local_storage.h"
#include "network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "network/node_info.h"
#include "constants/tips_id.h"
#include "comp/scene_comp.h"
#include "service/centre_scene_server_player_service.h"

#include "server_player_proto/centre_scene_server_player.pb.h"
///<<< END WRITING YOUR CODE
void ClientPlayerSceneServiceHandler::EnterSceneC2S(entt::entity player,
	const ::EnterSceneC2SRequest* request,
	::EnterSceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
        //如果是跨服副本服不能换场景
    if (const auto [server_type_] = tls.registry.get<GsNodeType>(global_entity());
    kRoomNode == server_type_ ||
        kRoomSceneCrossNode == server_type_)
    {
        response->mutable_error()->set_id(kRetEnterSceneServerType);
        return;
    }
    auto& scene_info = request->scene_info();
    if (scene_info.scene_confid() <= 0 && scene_info.guid() <= 0)
    {
        response->mutable_error()->set_id(kRetEnterSceneParamError);
        return;
    }
    //您当前就在这个场景，无需切换
    if (const auto current_scene = tls.registry.try_get<SceneEntity>(player); nullptr != current_scene)
    {
        if (const auto my_scene_info = tls.registry.try_get<SceneInfo>(current_scene->scene_entity_);
            nullptr != my_scene_info &&
            my_scene_info->guid() == scene_info.guid()
            && scene_info.guid() > 0)
        {
            response->mutable_error()->set_id(kRetEnterSceneYouInCurrentScene);
            return;
        }
    }
    CentreEnterSceneRequest message;
    message.mutable_scene_info()->CopyFrom(request->scene_info());
    Send2CentrePlayer(CentreScenePlayerServiceEnterSceneMsgId, message, player);
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushEnterSceneS2C(entt::entity player,
	const ::EnterSceneS2C* request,
	::EnterSceneS2C* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushSceneInfoS2C(entt::entity player,
	const ::SceneInfoS2C* request,
	::SceneInfoS2C* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

