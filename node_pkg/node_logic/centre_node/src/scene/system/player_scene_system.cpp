#include "player_scene_system.h"
#include "muduo/base/Logging.h"
#include "scene/comp/scene_comp.h"
#include "scene/system/scene_system.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/scene_error_tip.pb.h"
#include "node/comp/game_node_comp.h"
#include "node/centre_node_info.h"
#include "network/message_system.h"
#include "service_info/game_player_scene_service_info.h"
#include "service_info/game_service_service_info.h"
#include "scene/system/player_change_scene_system.h"
#include "player/system/player_tip_system.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/component/player_scene_comp.pb.h"
#include "proto/common/node.pb.h"

void PlayerSceneSystem::HandleLoginEnterScene(entt::entity playerEntity)
{
    if (!tls.registry.valid(playerEntity))
    {
        LOG_ERROR << "Player entity is not valid";
        return;
    }

    const auto* playerSceneInfo = tls.registry.try_get<PlayerSceneContextPBComponent>(playerEntity);
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
        if (kSuccess == SceneUtil::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info().guid() }, .enter = playerEntity }))
        {
            currentSceneId = entt::entity{ playerSceneInfo->scene_info().guid() };
        }
    }
    else if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfo->scene_info_last_time().guid() }))
    {
        if (kSuccess == SceneUtil::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info_last_time().guid() }, .enter = playerEntity }))
        {
            lastVisitedSceneId = entt::entity{ playerSceneInfo->scene_info_last_time().guid() };
        }
    }

    // Find appropriate scene for the player to enter
    if (currentSceneId == entt::null && lastVisitedSceneId == entt::null)
    {
        if (playerSceneInfo->scene_info().scene_confid() > 0)
        {
            currentSceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            if (currentSceneId == entt::null)
            {
                currentSceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            }
        }
    }

    // If still no valid scene found, fallback to default scene
    if (currentSceneId == entt::null)
    {
        currentSceneId = NodeSceneSystem::FindNotFullScene({ GetDefaultSceneConfigurationId() });
    }

    if (currentSceneId == entt::null)
    {
        LOG_ERROR << "Failed to find a scene for player login";
        return;
    }

    // Call method to handle player entering the game server
    ProcessPlayerEnterSceneNode(playerEntity, SceneUtil::GetGameNodeId(currentSceneId));

    // Prepare change scene information
    ChangeSceneInfoPBComponent changeSceneInfo;
    PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(changeSceneInfo, tls.sceneRegistry.get<SceneInfoPBComponent>(currentSceneId));
    changeSceneInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);
    changeSceneInfo.set_change_gs_status(ChangeSceneInfoPBComponent::eEnterGsSceneSucceed);
    PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeSceneInfo);
}

void PlayerSceneSystem::SendToGameNodeEnterScene(entt::entity playerEntity)
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

    const auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>((*playerSceneEntity).sceneEntity);
    if (!sceneInfo)
    {
        LOG_ERROR << "Scene info not found for player: " << playerId;
        return;
    }

    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "Player session not valid for player: " << playerId;
        return;
    }

    Centre2GsEnterSceneRequest request;
    request.set_scene_id(sceneInfo->guid());
    request.set_player_id(playerId);
    CallGameNodeMethod(GameServiceEnterSceneMessageId, request, playerNodeInfo->scene_node_id());

    LOG_DEBUG << "Player entered scene: " << playerId << ", Scene ID: " << sceneInfo->guid() << ", Game Node ID: " << playerNodeInfo->scene_node_id();
}

void PlayerSceneSystem::ProcessPlayerEnterSceneNode(entt::entity playerEntity, NodeId nodeId)
{
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "Player session not valid";
        return;
    }

    PlayerEnterGameNodeRequest request;
    request.set_player_id(tls.registry.get<Guid>(playerEntity));
    request.set_session_id(playerNodeInfo->gate_session_id());
    request.set_centre_node_id(GetNodeInfo().node_id());
    CallGameNodeMethod(GameServicePlayerEnterGameNodeMessageId, request, nodeId);
}

void PlayerSceneSystem::AttemptEnterNextScene(entt::entity playerEntity)
{
    auto playerId = tls.registry.get<Guid>(playerEntity);

	LOG_INFO << "Processing change scene queue for player: " << playerId;

	auto* changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(playerEntity);
	if (!changeSceneQueue)
	{
		LOG_ERROR << "Change scene queue not found for player: " << playerId;
		return;
	}

	const auto* fromSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
	if (!fromSceneEntity)
	{
		LOG_ERROR << "From scene entity not found for player: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYourSceneIsNull, {});
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
		toScene = NodeSceneSystem::FindNotFullScene(getSceneParams);
		if (toScene == entt::null)
		{
			LOG_WARN << "No available scene found for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneFull, {});
			PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
			return;
		}
		toSceneGuid = tls.registry.get<SceneInfoPBComponent>(toScene).guid();
	}
	else
	{
		toScene = entt::entity{ toSceneGuid };
		if (toScene == entt::null)
		{
			LOG_ERROR << "Target scene not found for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneNotFound, {});
			PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
			return;
		}
	}

	const auto* fromSceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(fromSceneEntity->sceneEntity);
	if (!fromSceneInfo)
	{
		LOG_ERROR << "From scene info not found for player: " << playerId;
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	const auto fromSceneGameNode = SceneUtil::get_game_node_eid(fromSceneInfo->guid());
	const auto toSceneGameNode = SceneUtil::get_game_node_eid(toSceneGuid);
	if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(fromSceneGameNode) || !tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(toSceneGameNode))
	{
		LOG_ERROR << "Game nodes not valid for scene change, from: " << fromSceneInfo->guid() << ", to: " << toSceneGuid;
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	if (toSceneGuid == fromSceneInfo->guid())
	{
		LOG_WARN << "Player is already in the current scene: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYouInCurrentScene, {});
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return;
	}

	if (!changeSceneInfo.ignore_full())
	{
		if (const auto ret = SceneUtil::CheckScenePlayerSize(toScene); ret != kSuccess)
		{
			LOG_WARN << "Scene player size check failed for player: " << playerId << ", ret: " << ret;
			PlayerTipSystem::SendToPlayer(playerEntity, ret, {});
			PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
			return;
		}
	}

	if (fromSceneGameNode == toSceneGameNode)
	{
		changeSceneInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs);
	}
	else
	{
		changeSceneInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);
	}

	LOG_INFO << "Change scene queue processed successfully for player: " << playerId;
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
}

uint32_t PlayerSceneSystem::GetDefaultSceneConfigurationId()
{
    return 1;
}
