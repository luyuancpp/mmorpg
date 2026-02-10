#include "player_room.h"
#include "muduo/base/Logging.h"
#include "modules/scene/comp/scene_comp.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "scene/system/player_change_room.h"
#include "player/system/player_tip.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/component/player_scene_comp.pb.h"
#include "network/node_message_utils.h"
#include "network/node_utils.h"
#include "node/system/zone_utils.h"
#include "engine/threading/node_context_manager.h"
#include "engine/threading/registry_manager.h"
#include <modules/scene/system/room_common.h>
#include "room_node_selector.h"
#include "room.h"

entt::entity PlayerSceneSystem::FindSceneForPlayerLogin(const PlayerSceneContextPBComponent& sceneContext)
{
	// 尝试进入上次成功进入的场景
	entt::entity currentSceneId = entt::entity{ sceneContext.scene_info().guid() };
	if (tlsRegistryManager.roomRegistry.valid(currentSceneId) &&
		kSuccess == RoomCommon::CheckPlayerEnterRoom({ .room = currentSceneId, .enter = entt::null }))
	{
		return currentSceneId;
	}

	// 尝试进入上次登录但未成功进入的场景
	entt::entity lastSceneId = entt::entity{ sceneContext.scene_info_last_time().guid() };
	if (tlsRegistryManager.roomRegistry.valid(lastSceneId) &&
		kSuccess == RoomCommon::CheckPlayerEnterRoom({ .room = lastSceneId, .enter = entt::null }))
	{
		return lastSceneId;
	}

	// 尝试找空闲的当前配置场景
	if (sceneContext.scene_info().scene_confid() > 0)
	{
		entt::entity candidate = RoomNodeSelectorSystem::SelectAvailableRoom({ sceneContext.scene_info().scene_confid() });
		if (candidate != entt::null)
			return candidate;
	}

	// fallback：默认配置场景
	return RoomNodeSelectorSystem::SelectAvailableRoom({ GetDefaultSceneConfigurationId() });
}


void PlayerSceneSystem::HandleLoginEnterScene(entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity)) {
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSceneInfo = tlsRegistryManager.actorRegistry.try_get<PlayerSceneContextPBComponent>(playerEntity);
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

    const auto* playerSceneEntity = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity);
    const auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
    if (!playerSceneEntity)
    {
        LOG_ERROR << "Player has not entered a scene: " << playerId;
        return;
    }

    const auto* sceneInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>((*playerSceneEntity).roomEntity);
    if (!sceneInfo)
    {
        LOG_ERROR << "Scene info not found for player: " << playerId;
        return;
    }

    const auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
    if (!sessionPB)
    {
        LOG_ERROR << "Player session not valid for player: " << playerId;
        return;
    }

	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(eNodeType::RoomNodeService);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << eNodeType::RoomNodeService
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

    Centre2GsEnterSceneRequest request;
    request.set_scene_id(sceneInfo->guid());
    request.set_player_id(playerId);
	CallRemoteMethodOnSession(SceneEnterSceneMessageId, request, it->second, eNodeType::RoomNodeService);

    LOG_DEBUG << "Player entered scene: " << playerId << ", Scene ID: " << sceneInfo->guid() << ", Game Node ID: " << it->second;
}

void PlayerSceneSystem::ProcessPlayerEnterSceneNode(entt::entity playerEntity, NodeId nodeId)
{
    const auto* info = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
    if (!info)
    {
        LOG_ERROR << "Player session not valid";
        return;
    }

    PlayerEnterGameNodeRequest request;
    request.set_player_id(tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity));
    request.set_session_id(info->gate_session_id());
    request.set_centre_node_id(GetNodeInfo().node_id());
	CallRemoteMethodOnSession(ScenePlayerEnterGameNodeMessageId, request, nodeId, eNodeType::RoomNodeService);
}

bool PlayerSceneSystem::VerifyChangeSceneRequest(entt::entity playerEntity)
{
	auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
	auto* queue = tlsRegistryManager.actorRegistry.try_get<ChangeSceneQueuePBComponent>(playerEntity);
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

	if (!tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity))
	{
		LOG_ERROR << "SceneEntityComp missing for player: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYourSceneIsNull, {});
		return false;
	}

	return true;
}


entt::entity PlayerSceneSystem::ResolveTargetScene(entt::entity playerEntity)
{
	auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
	auto& queue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity);
	auto& info = *queue.front();

	entt::entity toScene = entt::null;

	if (info.guid() > 0)
	{
		toScene = entt::entity{ info.guid() };
		if (!tlsRegistryManager.roomRegistry.valid(toScene))
		{
			LOG_ERROR << "Target scene not valid for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneNotFound, {});
			PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
			return entt::null;
		}
	}
	else
	{
		GetRoomParams params;
		params.roomConfigurationId = info.scene_confid();
		toScene = RoomNodeSelectorSystem::SelectLeastLoadedRoom(params);
		if (toScene == entt::null)
		{
			LOG_WARN << "No available scene found for player: " << playerId;
			PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneFull, {});
			PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
		}
	}

	return toScene;
}

bool PlayerSceneSystem::ValidateSceneSwitch(entt::entity playerEntity, entt::entity toScene)
{
	auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
	auto* fromSceneEntity = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity);
	const auto* fromSceneInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(fromSceneEntity->roomEntity);
	const auto* toSceneInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(toScene);

	if (!fromSceneInfo || !toSceneInfo)
	{
		LOG_ERROR << "Missing scene info from/to for player: " << playerId;
		PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	if (fromSceneInfo->guid() == toSceneInfo->guid())
	{
		LOG_WARN << "Already in the target scene: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneYouInCurrentScene, {});
		PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	auto& sceneNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::RoomNodeService);
	if (!sceneNodeRegistry.valid(RoomCommon::GetRoomNodeEntityId(fromSceneInfo->guid())) ||
		!sceneNodeRegistry.valid(RoomCommon::GetRoomNodeEntityId(toSceneInfo->guid())))
	{
		LOG_ERROR << "Invalid game node info for scene change";
		PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	const auto& changeInfo = *tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).front();
	if (!changeInfo.ignore_full() &&
		RoomCommon::HasRoomSlot(toScene) != kSuccess)
	{
		LOG_WARN << "Scene is full for player: " << playerId;
		PlayerTipSystem::SendToPlayer(playerEntity, kEnterSceneSceneFull, {});
		PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity);
		return false;
	}

	return true;
}

void PlayerSceneSystem::ProcessSceneChange(entt::entity playerEntity, entt::entity toScene)
{
	auto& changeInfo = *tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).front();
	auto* fromSceneComp = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity);

	auto fromNodeGuid = RoomCommon::GetGameNodeIdFromGuid(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneComp->roomEntity).guid());
	auto toNodeGuid = RoomCommon::GetGameNodeIdFromGuid(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(toScene).guid());

	entt::entity fromNode{fromNodeGuid};
	entt::entity toNode{ toNodeGuid };

	uint16_t fromZone = GetZoneIdFromNodeId(fromNodeGuid);
	uint16_t toZone = GetZoneIdFromNodeId(toNodeGuid);

	changeInfo.set_to_zone_id(toZone);

	if (fromZone == toZone) {
		changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eSameGs);
		changeInfo.set_is_cross_zone(false);
	}
	else {
		changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs);
		changeInfo.set_is_cross_zone(true);
	}

	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
}

void PlayerSceneSystem::HandleEnterScene(entt::entity playerEntity, const RoomInfoPBComponent& sceneInfo)
{
	ChangeRoomInfoPBComponent changeSceneInfo;
	PlayerChangeRoomUtil::CopySceneInfoToChangeInfo(changeSceneInfo, sceneInfo);
	if (const auto ret = PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeSceneInfo); ret != kSuccess)
	{
		LOG_ERROR << "Failed to push change scene info for player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity) << ": " << ret;
		PlayerTipSystem::SendToPlayer(playerEntity, ret, {});
		return;
	}

	PlayerSceneSystem::AttemptEnterNextScene(playerEntity);
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
	const auto nodeId = RoomSystem::GetGameNodeIdFromRoomEntity(sceneEntity);
	ProcessPlayerEnterSceneNode(playerEntity, nodeId);
}

void PlayerSceneSystem::PushInitialChangeSceneInfo(entt::entity playerEntity, entt::entity sceneEntity)
{
	const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(sceneEntity);

	ChangeRoomInfoPBComponent changeInfo;
	PlayerChangeRoomUtil::CopySceneInfoToChangeInfo(changeInfo, sceneInfo);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs);
	changeInfo.set_state(ChangeRoomInfoPBComponent::eEnterSucceed);

	PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo);
}
