#include "player_scene.h"
#include "muduo/base/Logging.h"
#include "centre_node.h"
#include "scene/comp/scene_comp.h"
#include "scene/util/scene_util.h"
#include "constants/tips_id_constants.h"
#include "game_logic/node/comp/game_node.h"
#include "game_logic/network/message_system.h"
#include "service/game_scene_server_player_service.h"
#include "service/game_service_service.h"
#include "game_logic/scene/util/player_change_scene.h"
#include "game_logic/player/util/player_tip_util.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/component/player_scene_comp.pb.h"

void PlayerSceneUtil::HandleLoginEnterScene(entt::entity playerEntity)
{
    if (!tls.registry.valid(playerEntity))
    {
        LOG_ERROR << "Player entity is not valid";
        return;
    }

    const auto* playerSceneInfo = tls.registry.try_get<PlayerSceneInfoComp>(playerEntity);
    if (!playerSceneInfo)
    {
        LOG_ERROR << "Player scene info not found";
        return;
    }

    entt::entity currentSceneId = entt::null;
    entt::entity lastVisitedSceneId = entt::null;

    // Check if previous scene is valid
    if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfo->scene_info().guid() }))
    {
        if (kOK == SceneUtil::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info().guid() }, .enter = playerEntity }))
        {
            currentSceneId = entt::entity{ playerSceneInfo->scene_info().guid() };
        }
    }
    else if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfo->scene_info_last_time().guid() }))
    {
        if (kOK == SceneUtil::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info_last_time().guid() }, .enter = playerEntity }))
        {
            lastVisitedSceneId = entt::entity{ playerSceneInfo->scene_info_last_time().guid() };
        }
    }

    // Find appropriate scene for the player to enter
    if (currentSceneId == entt::null && lastVisitedSceneId == entt::null)
    {
        if (playerSceneInfo->scene_info().scene_confid() > 0)
        {
            currentSceneId = NodeSceneUtil::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            if (currentSceneId == entt::null)
            {
                currentSceneId = NodeSceneUtil::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            }
        }
    }

    // If still no valid scene found, fallback to default scene
    if (currentSceneId == entt::null)
    {
        currentSceneId = NodeSceneUtil::FindNotFullScene({ GetDefaultSceneConfigurationId() });
    }

    if (currentSceneId == entt::null)
    {
        LOG_ERROR << "Failed to find a scene for player login";
        return;
    }

    // Call method to handle player entering the game server
    ProcessPlayerEnterGameServer(playerEntity, SceneUtil::GetGameNodeId(currentSceneId));

    // Prepare change scene information
    CentreChangeSceneInfo changeSceneInfo;
    PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(changeSceneInfo, tls.sceneRegistry.get<SceneInfo>(currentSceneId));
    changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
    changeSceneInfo.set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
    PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeSceneInfo);
}

void PlayerSceneUtil::SendToGameNodeEnterScene(entt::entity playerEntity)
{
    if (playerEntity == entt::null)
    {
        LOG_ERROR << "Player entity is null";
        return;
    }

    const auto* playerSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
    const auto playerId = tls.registry.get<Guid>(playerEntity);
    if (!playerSceneEntity)
    {
        LOG_ERROR << "Player has not entered a scene: " << playerId;
        return;
    }

    const auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfo>((*playerSceneEntity).sceneEntity);
    if (!sceneInfo)
    {
        LOG_ERROR << "Scene info not found for player: " << playerId;
        return;
    }

    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
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

void PlayerSceneUtil::ProcessPlayerEnterGameServer(entt::entity playerEntity, NodeId nodeId)
{
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "Player session not valid";
        return;
    }

    GameNodeEnterGsRequest request;
    request.set_player_id(tls.registry.get<Guid>(playerEntity));
    request.set_session_id(playerNodeInfo->gate_session_id());
    request.set_centre_node_id(gCentreNode ->GetNodeId());
    CallGameNodeMethod(GameServiceEnterGsMsgId, request, nodeId);
}

void PlayerSceneUtil::AttemptEnterNextScene(entt::entity playerEntity)
{
    auto playerId = tls.registry.get<Guid>(playerEntity);

	LOG_INFO << "Processing change scene queue for player: " << playerId;

	auto* changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueueComp>(playerEntity);
	if (!changeSceneQueue)
	{
		LOG_ERROR << "Change scene queue not found for player: " << playerId;
		return;
	}

	const auto* fromSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
	if (!fromSceneEntity)
	{
		LOG_ERROR << "From scene entity not found for player: " << playerId;
		PlayerTipUtil::SendToPlayer(playerEntity, kRetEnterSceneYourSceneIsNull, {});
		return;
	}

	auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();
	if (changeSceneInfo.processing())
	{
		LOG_WARN << "Change scene info is already processing for player: " << playerId;
		return;
	}

	changeSceneInfo.set_processing(true);
	auto toSceneGuid = changeSceneInfo.guid();
	entt::entity toScene = entt::null;

	if (toSceneGuid <= 0)
	{
		GetSceneParams getSceneParams;
		getSceneParams.sceneConfigurationId = changeSceneInfo.scene_confid();
		toScene = NodeSceneUtil::FindNotFullScene(getSceneParams);
		if (toScene == entt::null)
		{
			LOG_WARN << "No available scene found for player: " << playerId;
			PlayerTipUtil::SendToPlayer(playerEntity, kRetEnterSceneSceneFull, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
			return;
		}
		toSceneGuid = tls.registry.get<SceneInfo>(toScene).guid();
	}
	else
	{
		toScene = entt::entity{ toSceneGuid };
		if (toScene == entt::null)
		{
			LOG_ERROR << "Target scene not found for player: " << playerId;
			PlayerTipUtil::SendToPlayer(playerEntity, kRetEnterSceneSceneNotFound, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
			return;
		}
	}

	const auto* fromSceneInfo = tls.sceneRegistry.try_get<SceneInfo>(fromSceneEntity->sceneEntity);
	if (!fromSceneInfo)
	{
		LOG_ERROR << "From scene info not found for player: " << playerId;
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	const auto fromSceneGameNode = SceneUtil::get_game_node_eid(fromSceneInfo->guid());
	const auto toSceneGameNode = SceneUtil::get_game_node_eid(toSceneGuid);
	if (!tls.gameNodeRegistry.valid(fromSceneGameNode) || !tls.gameNodeRegistry.valid(toSceneGameNode))
	{
		LOG_ERROR << "Game nodes not valid for scene change, from: " << fromSceneInfo->guid() << ", to: " << toSceneGuid;
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	if (toSceneGuid == fromSceneInfo->guid())
	{
		LOG_WARN << "Player is already in the current scene: " << playerId;
		PlayerTipUtil::SendToPlayer(playerEntity, kRetEnterSceneYouInCurrentScene, {});
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	if (!changeSceneInfo.ignore_full())
	{
		if (const auto ret = SceneUtil::CheckScenePlayerSize(toScene); ret != kOK)
		{
			LOG_WARN << "Scene player size check failed for player: " << playerId << ", ret: " << ret;
			PlayerTipUtil::SendToPlayer(playerEntity, ret, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
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

	LOG_INFO << "Change scene queue processed successfully for player: " << playerId;
	PlayerChangeSceneSystem::ProcessChangeSceneQueue(playerEntity);
}

uint32_t PlayerSceneUtil::GetDefaultSceneConfigurationId()
{
    return 1;
}
