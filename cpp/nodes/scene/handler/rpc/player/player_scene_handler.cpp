
#include "player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "node/system/node/node.h"
#include "core/network/message_system.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "modules/scene/comp/scene_comp.h"
#include "proto/common/base/node.pb.h"
#include "proto/common/component/player_network_comp.pb.h"
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "grpc_client/scene_manager/scene_manager_service_grpc_client.h"
#include "engine/thread_context/node_context_manager.h"
#include "network/network_utils.h"
#include "network/node_utils.h"
#include "network/player_message_utils.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
///<<< END WRITING YOUR CODE

void SceneSceneClientPlayerHandler::EnterScene(entt::entity player,const ::EnterSceneC2SRequest* request,
	::EnterSceneC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_TRACE << "EnterSceneC2S request received for player: " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player)
		<< ", scene_info: " << request->scene_info().ShortDebugString();

	auto game_node_type = gNode->GetNodeInfo().scene_node_type();
	if (game_node_type == eSceneNodeType::kSceneNode ||
		game_node_type == eSceneNodeType::kSceneSceneCrossNode)
	{
		LOG_ERROR << "EnterSceneC2S request rejected due to server type: " << game_node_type;
		response->mutable_error_message()->set_id(kEnterSceneServerType);
		return;
	}

	const auto& scene_info = request->scene_info();
	if (scene_info.scene_config_id() <= 0 && scene_info.scene_id() <= 0)
	{
		LOG_ERROR << "EnterSceneC2S request rejected due to invalid scene_info: " << scene_info.ShortDebugString();
		response->mutable_error_message()->set_id(kEnterSceneParamError);
		return;
	}

	if (auto current_scene_comp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(player))
	{
		const auto current_scene_info = tlsEcs.sceneRegistry.try_get<SceneInfoComp>(current_scene_comp->sceneEntity);
		if (current_scene_info && current_scene_info->scene_id() == scene_info.scene_id() && scene_info.scene_id() > 0)
		{
			LOG_WARN << "Player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player) << " is already in the requested scene: " << scene_info.scene_id();
			response->mutable_error_message()->set_id(kEnterSceneYouInCurrentScene);
			return;
		}
	}

	const auto* playerSessionPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (!playerSessionPB)
	{
		LOG_ERROR << "EnterSceneC2S: PlayerSessionSnapshotComp missing for player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player);
		response->mutable_error_message()->set_id(kEnterSceneParamError);
		return;
	}

	// Resolve gate info
	NodeId gateNodeId = GetGateNodeId(playerSessionPB->gate_session_id());
	std::string gateInstanceId;
	auto& gateRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	if (const auto* gateNodeInfo = gateRegistry.try_get<NodeInfo>(entt::entity{gateNodeId}))
	{
		gateInstanceId = gateNodeInfo->node_uuid();
	}

	// Find a SceneManager gRPC node
	auto smEntity = GetSceneManagerEntity(playerSessionPB->player_id());
	if (smEntity == entt::null)
	{
		LOG_ERROR << "EnterSceneC2S: No SceneManager node available for player " << playerSessionPB->player_id();
		response->mutable_error_message()->set_id(kEnterSceneParamError);
		return;
	}

	// Build EnterSceneRequest for SceneManager
	auto &smRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneManagerNodeService);
	::scene_manager::EnterSceneRequest req;
	req.set_player_id(playerSessionPB->player_id());
	req.set_scene_id(scene_info.scene_id());
	req.set_scene_conf_id(scene_info.scene_config_id());
	req.set_session_id(playerSessionPB->gate_session_id());
	req.set_gate_id(std::to_string(gateNodeId));
	req.set_gate_instance_id(gateInstanceId);
	req.set_gate_zone_id(GetZoneId());
	req.set_zone_id(GetZoneId());

	scene_manager::SendSceneManagerEnterScene(smRegistry, smEntity, req);

	LOG_INFO << "EnterSceneC2S: Sent EnterScene to SceneManager for player " << playerSessionPB->player_id()
			 << ", scene_config_id=" << scene_info.scene_config_id()
			 << ", scene_id=" << scene_info.scene_id();
///<<< END WRITING YOUR CODE

}

void SceneSceneClientPlayerHandler::NotifyEnterScene(entt::entity player,const ::EnterSceneS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, *request, player);
///<<< END WRITING YOUR CODE

}

void SceneSceneClientPlayerHandler::SceneInfoC2S(entt::entity player,const ::SceneInfoRequest* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	auto* sceneEntityComp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(player);
	if (!sceneEntityComp || sceneEntityComp->sceneEntity == entt::null)
	{
		LOG_WARN << "SceneInfoC2S: Player not in any scene";
		return;
	}

	const auto* sceneInfo = tlsEcs.sceneRegistry.try_get<SceneInfoComp>(sceneEntityComp->sceneEntity);
	if (!sceneInfo)
	{
		LOG_WARN << "SceneInfoC2S: Scene info not found for player's scene entity";
		return;
	}

	SceneInfoS2C message;
	message.add_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifySceneInfoMessageId, message, player);
///<<< END WRITING YOUR CODE

}

void SceneSceneClientPlayerHandler::NotifySceneInfo(entt::entity player,const ::SceneInfoS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
SendMessageToClientViaGate(SceneSceneClientPlayerNotifySceneInfoMessageId, *request, player);
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
