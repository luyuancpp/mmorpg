
#include "player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "scene_node.h"
#include "core/network/message_system.h"
#include "proto/table/tip/scene_error_tip.pb.h"
#include "proto/centre/centre_player_scene.pb.h"
#include "scene/comp/scene_comp.h"
#include "service_info/centre_player_scene_service_info.h"

#include "proto/common/node.pb.h"
#include "network/node_message_utils.h"
///<<< END WRITING YOUR CODE



void SceneSceneClientPlayerHandler::EnterScene(entt::entity player,const ::EnterSceneC2SRequest* request,
	::EnterSceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_TRACE << "EnterSceneC2S request received for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player)
		<< ", scene_info: " << request->scene_info().DebugString();

	auto game_node_type = gNode->GetNodeInfo().scene_node_type();
	if (game_node_type == eSceneNodeType::kRoomNode ||
		game_node_type == eSceneNodeType::kRoomSceneCrossNode)
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

	if (auto current_scene_comp = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(player))
	{
		const auto current_scene_info = tlsRegistryManager.actorRegistry.try_get<SceneInfoPBComponent>(current_scene_comp->sceneEntity);
		if (current_scene_info && current_scene_info->guid() == scene_info.guid() && scene_info.guid() > 0)
		{
			LOG_WARN << "Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " is already in the requested scene: " << scene_info.guid();
			response->mutable_error_message()->set_id(kEnterSceneYouInCurrentScene);
			return;
		}
	}

	CentreEnterSceneRequest enterSceneReq;
	enterSceneReq.mutable_scene_info()->CopyFrom(scene_info);
	SendToCentrePlayerByClientNode(CentrePlayerSceneEnterSceneMessageId, enterSceneReq, player);

	LOG_TRACE << "EnterSceneC2S request processed successfully for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player);
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifyEnterScene(entt::entity player,const ::EnterSceneS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::SceneInfoC2S(entt::entity player,const ::SceneInfoRequest* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    SceneInfoRequest req;
	SendToCentrePlayerByClientNode(CentrePlayerSceneSceneInfoC2SMessageId, req, player);
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifySceneInfo(entt::entity player,const ::SceneInfoS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifyActorCreate(entt::entity player,const ::ActorCreateS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifyActorDestroy(entt::entity player,const ::ActorDestroyS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifyActorListCreate(entt::entity player,const ::ActorListCreateS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSceneClientPlayerHandler::NotifyActorListDestroy(entt::entity player,const ::ActorListDestroyS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

