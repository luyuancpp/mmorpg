#include "player_scene.h"
#include "muduo/base/Logging.h"
#include "centre_node.h"
#include "comp/scene.h"
#include "system/scene/scene_system.h"
#include "constants/tips_id.h"
#include "comp/game_node.h"
#include "network/message_system.h"
#include "service/game_scene_server_player_service.h"
#include "service/game_service_service.h"
#include "system/player_change_scene.h"
#include "system/player_tip.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/component/player_scene_comp.pb.h"

void PlayerSceneSystem::OnLoginEnterScene(entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player not found";
		return;
	}

	const auto* playerSceneInfoComp = tls.registry.try_get<PlayerSceneInfoComp>(player);
	if (!playerSceneInfoComp)
	{
		LOG_ERROR << "Player scene info not found";
		return;
	}

	entt::entity sceneId = entt::null;
	entt::entity lastSceneId = entt::null;

	// Check if previous scene is valid
	if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfoComp->scene_info().guid() }))
	{
		if (kOK == ScenesSystem::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfoComp->scene_info().guid() }, .enter = player }))
		{
			sceneId = entt::entity{ playerSceneInfoComp->scene_info().guid() };
		}
	}
	else if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfoComp->scene_info_last_time().guid() }))
	{
		if (kOK == ScenesSystem::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfoComp->scene_info_last_time().guid() }, .enter = player }))
		{
			lastSceneId = entt::entity{ playerSceneInfoComp->scene_info_last_time().guid() };
		}
	}

	// Find appropriate scene for the player to enter
	if (sceneId == entt::null && lastSceneId == entt::null)
	{
		if (playerSceneInfoComp->scene_info().scene_confid() > 0)
		{
			sceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfoComp->scene_info().scene_confid() });
			if (sceneId == entt::null)
			{
				sceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfoComp->scene_info().scene_confid() });
			}
		}
	}

	// If still no valid scene found, fallback to default scene
	if (sceneId == entt::null)
	{
		sceneId = NodeSceneSystem::FindNotFullScene({ GetDefaultSceneConfigId() });
	}

	if (sceneId == entt::null)
	{
		LOG_ERROR << "Failed to find a scene for player login";
		return;
	}

	// Call method to handle player entering the game server
	CallPlayerEnterGs(player, ScenesSystem::GetGameNodeId(sceneId));

	// Prepare change scene information
	CentreChangeSceneInfo changeSceneInfo;
	PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(changeSceneInfo, tls.sceneRegistry.get<SceneInfo>(sceneId));
	changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
	changeSceneInfo.set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::PushChangeSceneInfo(player, changeSceneInfo);
}

void PlayerSceneSystem::Send2GsEnterScene(entt::entity player)
{
	if (player == entt::null)
	{
		LOG_ERROR << "Player entity is null";
		return;
	}

	const auto* pScene = tls.registry.try_get<SceneEntity>(player);
	const auto playerId = tls.registry.get<Guid>(player);
	if (!pScene)
	{
		LOG_ERROR << "Player has not entered a scene: " << playerId;
		return;
	}

	const auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfo>((*pScene).sceneEntity);
	if (!sceneInfo)
	{
		LOG_ERROR << "Scene info not found for player: " << playerId;
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player session not valid for player: " << playerId;
		return;
	}

	Centre2GsEnterSceneRequest request;
	request.set_scene_id(sceneInfo->guid());
	request.set_player_id(playerId);
	CallGameNodeMethod(GameServiceEnterSceneMsgId, request, playerNodeInfo->game_node_id());

	LOG_DEBUG << "Player entered scene: " << playerId << ", Scene ID: " << sceneInfo->guid() << ", Game Node ID: " << playerNodeInfo->game_node_id();
}

void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId nodeId)
{
	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player session not valid";
		return;
	}

	GameNodeEnterGsRequest request;
	request.set_player_id(tls.registry.get<Guid>(player));
	request.set_session_id(playerNodeInfo->gate_session_id());
	request.set_centre_node_id(g_centre_node->GetNodeId());
	CallGameNodeMethod(GameServiceEnterGsMsgId, request, nodeId);
}

void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
	auto* changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!changeSceneQueue)
	{
		return;
	}

	const auto* fromScene = tls.registry.try_get<SceneEntity>(player);
	if (!fromScene)
	{
		PlayerTipSystem::Tip(player, kRetEnterSceneYourSceneIsNull, {});
		return;
	}

	auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();
	if (changeSceneInfo.processing())
	{
		return;
	}

	changeSceneInfo.set_processing(true);
	auto toSceneGuid = changeSceneInfo.guid();
	entt::entity toScene = entt::null;

	if (toSceneGuid <= 0)
	{
		GetSceneParam getSceneParam;
		getSceneParam.sceneConfId = changeSceneInfo.scene_confid();
		toScene = NodeSceneSystem::FindNotFullScene(getSceneParam);
		if (toScene == entt::null)
		{
			PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
		toSceneGuid = tls.registry.get<SceneInfo>(toScene).guid();
	}
	else
	{
		toScene = entt::entity{ toSceneGuid };
		if (toScene == entt::null)
		{
			PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
	}

	const auto* fromSceneInfo = tls.sceneRegistry.try_get<SceneInfo>(fromScene->sceneEntity);
	if (!fromSceneInfo)
	{
		return;
	}

	const auto fromSceneGameNode = ScenesSystem::get_game_node_eid(fromSceneInfo->guid());
	const auto toSceneGameNode = ScenesSystem::get_game_node_eid(toSceneGuid);
	if (!tls.gameNodeRegistry.valid(fromSceneGameNode) || !tls.gameNodeRegistry.valid(toSceneGameNode))
	{
		LOG_ERROR << "Scene not found: " << fromSceneInfo->guid() << " to " << toSceneGuid;
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}

	if (toSceneGuid == fromSceneInfo->guid())
	{
		PlayerTipSystem::Tip(player, kRetEnterSceneYouInCurrentScene, {});
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}

	if (!changeSceneInfo.ignore_full())
	{
		if (const auto ret = ScenesSystem::CheckScenePlayerSize(toScene); ret != kOK)
		{
			PlayerTipSystem::Tip(player, ret, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
	}

	if (fromSceneGameNode == toSceneGameNode)
	{
		changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs);
	}
	else
	{
		changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
	}

	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
}

uint32_t PlayerSceneSystem::GetDefaultSceneConfigId()
{
	return 1;
}
