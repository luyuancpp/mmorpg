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

entt::entity PlayerSceneSystem::FindSceneForPlayerLogin(const PlayerSceneContextPBComponent& sceneContext)
{
	// 尝试进入上次成功进入的场景
	entt::entity currentSceneId = entt::entity{ sceneContext.scene_info().guid() };
	if (tls.sceneRegistry.valid(currentSceneId) &&
		kSuccess == SceneUtil::CheckPlayerEnterScene({ .scene = currentSceneId, .enter = entt::null }))
	{
		return currentSceneId;
	}

	// 尝试进入上次登录但未成功进入的场景
	entt::entity lastSceneId = entt::entity{ sceneContext.scene_info_last_time().guid() };
	if (tls.sceneRegistry.valid(lastSceneId) &&
		kSuccess == SceneUtil::CheckPlayerEnterScene({ .scene = lastSceneId, .enter = entt::null }))
	{
		return lastSceneId;
	}

	// 尝试找空闲的当前配置场景
	if (sceneContext.scene_info().scene_confid() > 0)
	{
		entt::entity candidate = NodeSceneSystem::FindNotFullScene({ sceneContext.scene_info().scene_confid() });
		if (candidate != entt::null)
			return candidate;
	}

	// fallback：默认配置场景
	return NodeSceneSystem::FindNotFullScene({ GetDefaultSceneConfigurationId() });
}


void PlayerSceneSystem::HandleLoginEnterScene(entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity)) {
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSceneInfo = tls.registry.try_get<PlayerSceneContextPBComponent>(playerEntity);
	if (!playerSceneInfo) {
		LOG_ERROR << "Player scene info not found";
		return;
	}

	entt::entity targetSceneId = FindSceneForPlayerLogin(*playerSceneInfo);
	if (targetSceneId == entt::null) {
		LOG_ERROR << "Failed to find a scene for player login";
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneFailed, {});
		return;
	}

	ProcessEnterGameNode(playerEntity, targetSceneId);
	PushInitialChangeSceneInfo(playerEntity, targetSceneId);
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

    const auto* playerSessionSnapshotPB = tls.registry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
    if (!playerSessionSnapshotPB)
    {
        LOG_ERROR << "Player session not valid for player: " << playerId;
        return;
    }

    Centre2GsEnterSceneRequest request;
    request.set_scene_id(sceneInfo->guid());
    request.set_player_id(playerId);
    CallGameNodeMethod(SceneEnterSceneMessageId, request, playerSessionSnapshotPB->scene_node_id());

    LOG_DEBUG << "Player entered scene: " << playerId << ", Scene ID: " << sceneInfo->guid() << ", Game Node ID: " << playerSessionSnapshotPB->scene_node_id();
}

void PlayerSceneSystem::ProcessPlayerEnterSceneNode(entt::entity playerEntity, NodeId nodeId)
{
    const auto* info = tls.registry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
    if (!info)
    {
        LOG_ERROR << "Player session not valid";
        return;
    }

    PlayerEnterGameNodeRequest request;
    request.set_player_id(tls.registry.get<Guid>(playerEntity));
    request.set_session_id(info->gate_session_id());
    request.set_centre_node_id(GetNodeInfo().node_id());
    CallGameNodeMethod(ScenePlayerEnterGameNodeMessageId, request, nodeId);
}

bool PlayerSceneSystem::VerifyChangeSceneRequest(entt::entity playerEntity)
{
	auto playerId = tls.registry.get<Guid>(playerEntity);
	auto* queue = tls.registry.try_get<ChangeSceneQueuePBComponent>(playerEntity);
	if (!queue || queue->empty())
	{
		LOG_WARN << "Change scene queue is empty for player: " << playerId;
		return false;
	}

	auto& info = *queue->front();
	if (info.processing())
	{
		LOG_WARN << "Scene change already in progress for player: " << playerId;
		return false;
	}

	info.set_processing(true);

	if (!tls.registry.try_get<SceneEntityComp>(playerEntity))
	{
		LOG_ERROR << "SceneEntityComp missing for player: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYourSceneIsNull, {});
		return false;
	}

	return true;
}


entt::entity PlayerSceneSystem::ResolveTargetScene(entt::entity playerEntity)
{
	auto playerId = tls.registry.get<Guid>(playerEntity);
	auto& queue = tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity);
	auto& info = *queue.front();

	entt::entity toScene = entt::null;

	if (info.guid() > 0)
	{
		toScene = entt::entity{ info.guid() };
		if (!tls.sceneRegistry.valid(toScene))
		{
			LOG_ERROR << "Target scene not valid for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneNotFound, {});
			PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
			return entt::null;
		}
	}
	else
	{
		GetSceneParams params;
		params.sceneConfigurationId = info.scene_confid();
		toScene = NodeSceneSystem::FindSceneWithMinPlayerCount(params);
		if (toScene == entt::null)
		{
			LOG_WARN << "No available scene found for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneFull, {});
			PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		}
	}

	return toScene;
}

bool PlayerSceneSystem::ValidateSceneSwitch(entt::entity playerEntity, entt::entity toScene)
{
	auto playerId = tls.registry.get<Guid>(playerEntity);
	auto* fromSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
	const auto* fromSceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(fromSceneEntity->sceneEntity);
	const auto* toSceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(toScene);

	if (!fromSceneInfo || !toSceneInfo)
	{
		LOG_ERROR << "Missing scene info from/to for player: " << playerId;
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	if (fromSceneInfo->guid() == toSceneInfo->guid())
	{
		LOG_WARN << "Already in the target scene: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYouInCurrentScene, {});
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(SceneUtil::get_game_node_eid(fromSceneInfo->guid())) ||
		!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(SceneUtil::get_game_node_eid(toSceneInfo->guid())))
	{
		LOG_ERROR << "Invalid game node info for scene change";
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	const auto& changeInfo = *tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).front();
	if (!changeInfo.ignore_full() &&
		SceneUtil::CheckScenePlayerSize(toScene) != kSuccess)
	{
		LOG_WARN << "Scene is full for player: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneFull, {});
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	return true;
}

void PlayerSceneSystem::ProcessSceneChange(entt::entity playerEntity, entt::entity toScene)
{
	auto& changeInfo = *tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).front();
	auto* fromSceneComp = tls.registry.try_get<SceneEntityComp>(playerEntity);

	const auto fromNode = SceneUtil::get_game_node_eid(tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneComp->sceneEntity).guid());
	const auto toNode = SceneUtil::get_game_node_eid(tls.sceneRegistry.get<SceneInfoPBComponent>(toScene).guid());

	if (fromNode == toNode)
		changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs);
	else
		changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);

	PlayerChangeSceneUtil::AdvanceSceneChangeState(playerEntity);
}

void PlayerSceneSystem::AttemptEnterNextScene(entt::entity playerEntity)
{
	// 1. 检查前置状态（队列、玩家、场景）
	if (!VerifyChangeSceneRequest(playerEntity))
		return;

	// 2. 查找目标场景（已有 or 分配）
	auto toScene = ResolveTargetScene(playerEntity);
	if (toScene == entt::null)
		return;

	// 3. 验证是否允许切换（节点、容量、重复）
	if (!ValidateSceneSwitch(playerEntity, toScene))
		return;

	// 4. 发起切换
	ProcessSceneChange(playerEntity, toScene);
}


uint32_t PlayerSceneSystem::GetDefaultSceneConfigurationId()
{
    return 1;
}

void PlayerSceneSystem::ProcessEnterGameNode(entt::entity playerEntity, entt::entity sceneEntity)
{
	const auto nodeId = SceneUtil::GetGameNodeId(sceneEntity);
	ProcessPlayerEnterSceneNode(playerEntity, nodeId);
}

void PlayerSceneSystem::PushInitialChangeSceneInfo(entt::entity playerEntity, entt::entity sceneEntity)
{
	const auto& sceneInfo = tls.sceneRegistry.get<SceneInfoPBComponent>(sceneEntity);

	ChangeSceneInfoPBComponent changeInfo;
	PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(changeInfo, sceneInfo);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);
	changeInfo.set_state(ChangeSceneInfoPBComponent::eEnterSucceed);

	PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo);
}
