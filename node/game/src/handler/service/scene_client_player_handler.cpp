#include "scene_client_player_handler.h"
#include "thread_local/storage.h"
#include "system/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "constants/tips_id.h"
#include "comp/scene.h"
#include "service/centre_scene_server_player_service.h"
#include "proto/logic/server_player/centre_scene_server_player.pb.h"
#include "game_node.h"

#include "proto/logic/constants/node.pb.h"
///<<< END WRITING YOUR CODE
void ClientPlayerSceneServiceHandler::EnterSceneC2S(entt::entity player,
	const ::EnterSceneC2SRequest* request,
	::EnterSceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	auto game_node_type = gGameNode->GetNodeType();
	if (game_node_type == eGameNodeType::kRoomNode ||
		game_node_type == eGameNodeType::kRoomSceneCrossNode)
	{
		response->mutable_error()->set_id(kRetEnterSceneServerType);
		return;
	}

	const auto& scene_info = request->scene_info();
	if (scene_info.scene_confid() <= 0 && scene_info.guid() <= 0)
	{
		response->mutable_error()->set_id(kRetEnterSceneParamError);
		return;
	}

	if (auto current_scene_comp = tls.registry.try_get<SceneEntityComp>(player))
	{
		const auto current_scene_info = tls.registry.try_get<SceneInfo>(current_scene_comp->sceneEntity);
		if (current_scene_info && current_scene_info->guid() == scene_info.guid() && scene_info.guid() > 0)
		{
			response->mutable_error()->set_id(kRetEnterSceneYouInCurrentScene);
			return;
		}
	}

	CentreEnterSceneRequest rq;
	rq.mutable_scene_info()->CopyFrom(scene_info);
	SendToCentrePlayerById(CentreScenePlayerServiceEnterSceneMsgId, rq, player);
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushEnterSceneS2C(entt::entity player,
	const ::EnterSceneS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::SceneInfoC2S(entt::entity player,
	const ::SceneInfoRequest* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    SceneInfoRequest rq;
    SendToCentrePlayerById(CentreScenePlayerServiceSceneInfoC2SMsgId, rq, player);
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushSceneInfoS2C(entt::entity player,
	const ::SceneInfoS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushActorCreateS2C(entt::entity player,
	const ::ActorCreateS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushActorDestroyS2C(entt::entity player,
	const ::ActorDestroyS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushActorListCreateS2C(entt::entity player,
	const ::ActorListCreateS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::PushActorListDestroyS2C(entt::entity player,
	const ::ActorListDestroyS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

