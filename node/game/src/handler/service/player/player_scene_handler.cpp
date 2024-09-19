#include "player_scene_handler.h"
///<<< BEGIN WRITING YOUR CODE

#include "game_node.h"
#include "game_logic/network/message_util.h"
#include "pbc/scene_error_tip.pb.h"
#include "proto/logic/server_player/centre_player_scene.pb.h"
#include "scene/comp/scene_comp.h"
#include "service_info/centre_player_scene_service_info.h"

#include "proto/logic/constants/node.pb.h"
///<<< END WRITING YOUR CODE
void ClientPlayerSceneServiceHandler::EnterScene(entt::entity player,const ::EnterSceneC2SRequest* request,
	     EnterSceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_TRACE << "EnterSceneC2S request received for player: " << tls.registry.get<Guid>(player)
		<< ", scene_info: " << request->scene_info().DebugString();

	auto game_node_type = gGameNode->GetNodeType();
	if (game_node_type == eGameNodeType::kRoomNode ||
		game_node_type == eGameNodeType::kRoomSceneCrossNode)
	{
		LOG_ERROR << "EnterSceneC2S request rejected due to server type: " << game_node_type;
		response->mutable_error_message()->set_id(kEnterSceneServerType);
		return;
	}

	const auto& scene_info = request->scene_info();
	if (scene_info.scene_confid() <= 0 && scene_info.guid() <= 0)
	{
		LOG_ERROR << "EnterSceneC2S request rejected due to invalid scene_info: " << scene_info.DebugString();
		response->mutable_error_message()->set_id(kEnterSceneParamError);
		return;
	}

	if (auto current_scene_comp = tls.registry.try_get<SceneEntityComp>(player))
	{
		const auto current_scene_info = tls.registry.try_get<SceneInfoPBComp>(current_scene_comp->sceneEntity);
		if (current_scene_info && current_scene_info->guid() == scene_info.guid() && scene_info.guid() > 0)
		{
			LOG_WARN << "Player " << tls.registry.get<Guid>(player) << " is already in the requested scene: " << scene_info.guid();
			response->mutable_error_message()->set_id(kEnterSceneYouInCurrentScene);
			return;
		}
	}

	CentreEnterSceneRequest rq;
	rq.mutable_scene_info()->CopyFrom(scene_info);
	SendToCentrePlayerById(CentrePlayerSceneServiceEnterSceneMessageId, rq, player);

	LOG_TRACE << "EnterSceneC2S request processed successfully for player: " << tls.registry.get<Guid>(player);
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifyEnterScene(entt::entity player,const ::EnterSceneS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::SceneInfoC2S(entt::entity player,const ::SceneInfoRequest* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    SceneInfoRequest rq;
    SendToCentrePlayerById(CentrePlayerSceneServiceSceneInfoC2SMessageId, rq, player);
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifySceneInfo(entt::entity player,const ::SceneInfoS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifyActorCreate(entt::entity player,const ::ActorCreateS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifyActorDestroy(entt::entity player,const ::ActorDestroyS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifyActorListCreate(entt::entity player,const ::ActorListCreateS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ClientPlayerSceneServiceHandler::NotifyActorListDestroy(entt::entity player,const ::ActorListDestroyS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

