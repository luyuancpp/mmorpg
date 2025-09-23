#include "player_lifecycle.h"
#include "muduo/base/Logging.h"

#include "modules/scene/comp/scene_comp.h"
#include "session/system/session.h"
#include "player/comp/player.h"
#include "scene/system/player_change_room.h"
#include "scene/system/player_room.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "scene/system/room.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "engine/threading/redis_manager.h"
#include "engine/core/utils/defer/defer.h"
#include "proto/common/node.pb.h"
#include "table/code/constants/global_abnormal_logout_table_id_constants.h"
#include "table/code/globalvariable_table.h"
#include "proto/logic/component/player_scene_comp.pb.h"
#include "proto/logic/database/mysql_database_table.pb.h"
#include "engine/core/network/network_utils.h"
#include "engine/core/network/node_message_utils.h"
#include "engine/core/network/rpc_session.h"
#include "engine/core/network/player_message_utils.h"
#include "engine/core/type_alias/player_session_type_alias.h"
#include "engine/threading/node_context_manager.h"
#include "engine/threading/player_manager.h"
#include <modules/scene/system/room_common.h>

void PlayerLifecycleSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_centre_database& playerData, const std::any& extra)
{
	//load 回来之前断开连接了,然后又加到redis了 这种怎么办,session id 变了咋办

	LOG_INFO << "Handling async load for player: " << playerId;
	assert(tlsPlayerList.find(playerId) == tlsPlayerList.end());

	auto sessionPbComp = std::any_cast<PlayerSessionSnapshotPBComp>(extra);

	if (GlobalSessionList().find(sessionPbComp.gate_session_id()) == GlobalSessionList().end())
	{
		return;
	}
	
	auto playerEntity = tlsRegistryManager.actorRegistry.create();
	if (const auto [first, success] = tlsPlayerList.emplace(playerId, playerEntity); !success)
	{
		LOG_ERROR << "Error emplacing player in player list: " << playerId;
		return;
	}

	auto& sessionPB = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(playerEntity, std::move(sessionPbComp));

	tlsRegistryManager.actorRegistry.emplace<Player>(playerEntity);
	tlsRegistryManager.actorRegistry.emplace<Guid>(playerEntity, playerId);
	tlsRegistryManager.actorRegistry.emplace<PlayerSceneContextPBComponent>(playerEntity, playerData.scene_info());

	tlsRegistryManager.actorRegistry.emplace<PlayerEnterGameStatePbComp>(playerEntity).set_enter_gs_type(LOGIN_FIRST);

	PlayerSceneSystem::HandleLoginEnterScene(playerEntity);
	LOG_INFO << "Player login enter scene handled: " << playerId;
}

void PlayerLifecycleSystem::HandlePlayerAsyncSaved(Guid playerId, player_centre_database& playerData)
{
	LOG_INFO << "Handling async save for player: " << playerId;
	// Placeholder for handling saved player data asynchronously
}

void PlayerLifecycleSystem::ProcessPlayerSessionState(entt::entity player)
{
	if (const auto* const enterGameFlag = tlsRegistryManager.actorRegistry.try_get<PlayerEnterGameStatePbComp>(player))
	{
		LOG_DEBUG << "EnterGameNodeInfoPBComponent found with type: " << enterGameFlag->enter_gs_type();

		if (enterGameFlag->enter_gs_type() != LOGIN_NONE && enterGameFlag->enter_gs_type() != LOGIN_RECONNECT)
		{
			PlayerLifecycleSystem::HandlePlayerLogin(player);
		}
		else
		{
			PlayerLifecycleSystem::HandlePlayerReconnection(player);
		}

		tlsRegistryManager.actorRegistry.remove<PlayerEnterGameStatePbComp>(player);
		LOG_DEBUG << "Removed EnterGameNodeInfoPBComponent from player";
	}
}

void PlayerLifecycleSystem::HandlePlayerLogin(entt::entity playerEntity)
{
	const auto enterGameFlag = tlsRegistryManager.actorRegistry.try_get<PlayerEnterGameStatePbComp>(playerEntity);
	if (!enterGameFlag)
	{
		LOG_WARN << "HandlePlayerLogin called but EnterGameNodeInfoPBComponent not found";
		return;
	}

	LOG_INFO << "Handling player login for entity: " << static_cast<uint32_t>(playerEntity);

	Centre2GsLoginRequest message;
	message.set_enter_gs_type(enterGameFlag->enter_gs_type());

	SendMessageToPlayerOnSceneNode(ScenePlayerCentre2GsLoginMessageId, message, playerEntity);
	LOG_DEBUG << "Sent Centre2GsLoginRequest to game scene";
}

void PlayerLifecycleSystem::HandlePlayerReconnection(entt::entity player)
{
	LOG_INFO << "Handling player reconnection for entity: " << static_cast<uint32_t>(player);
}

void PlayerLifecycleSystem::AddGameNodePlayerToGateNode(entt::entity playerEntity)
{
	auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB)
	{
		LOG_WARN << "PlayerSessionSnapshotPB not found for player: " << tlsRegistryManager.actorRegistry.try_get<Guid>(playerEntity);
		return;
	}

	LOG_INFO << "Adding game node player to gate node, session_id: " << sessionPB->gate_session_id();

	entt::entity gateNodeId{ GetGateNodeId(sessionPB->gate_session_id()) };
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	if (!registry.valid(gateNodeId))
	{
		LOG_WARN << "Gate node invalid for session_id: " << sessionPB->gate_session_id();
		return;
	}

	auto gateNodeScene = registry.try_get<RpcSession>(gateNodeId);
	if (!gateNodeScene)
	{
		LOG_WARN << "Gate node RpcSession not found for session_id: " << sessionPB->gate_session_id();
		return;
	}

	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(eNodeType::SceneNodeService);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << eNodeType::SceneNodeService
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	RegisterGameNodeSessionRequest request;
	request.mutable_session_info()->set_session_id(sessionPB->gate_session_id());
	request.set_scene_node_id(it->second);
	gateNodeScene->CallRemoteMethod(GatePlayerEnterGameNodeMessageId, request);

	LOG_DEBUG << "Called remote method GatePlayerEnterGameNodeMessageId for session_id: " << sessionPB->gate_session_id();
}

void PlayerLifecycleSystem::HandleRoomNodePlayerRegisteredAtGate(entt::entity playerEntity)
{
	const auto* const sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB)
	{
		LOG_ERROR << "Invalid player session in HandleGameNodePlayerRegisteredAtGateNode";
		return;
	}
	const auto* const playerId = tlsRegistryManager.actorRegistry.try_get<Guid>(playerEntity);
	if (!playerId)
	{
		LOG_ERROR << "Player ID not found in HandleGameNodePlayerRegisteredAtGateNode";
		return;
	}
	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(eNodeType::SceneNodeService);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << eNodeType::SceneNodeService
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	LOG_INFO << "Player registered at gate node, session: " << sessionPB->gate_session_id() << ", playerId: " << *playerId;

	RegisterPlayerSessionRequest request;
	request.set_session_id(sessionPB->gate_session_id());
	request.set_player_id(*playerId);
	SendMessageToSessionNode(SceneUpdateSessionDetailMessageId, request, it->second, eNodeType::SceneNodeService);
	LOG_DEBUG << "Sent session update to scene node";
}

void PlayerLifecycleSystem::HandleNormalExit(Guid playerID)
{
	LOG_INFO << "Handling normal exit for player: " << playerID;
	Logout(playerID);
}

void PlayerLifecycleSystem::HandleAbnormalExit(Guid playerID)
{
	LOG_INFO << "Handling abnormal exit for player: " << playerID;

	const auto playerEntity = GetPlayer(playerID);
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity not valid for abnormal exit: " << playerID;
		return;
	}

	FetchGlobalVariableTableOrReturnVoid(kGlobalVariable_Abnormal_logout);

	LOG_DEBUG << "Starting abnormal logout timer for player: " << playerID;
	tlsRegistryManager.actorRegistry.get_or_emplace<AbnormalExitTimer>(playerEntity).timer.RunAfter(globalVariableTable->todouble(), [playerID]() {
		LOG_INFO << "Abnormal exit timeout reached for player: " << playerID;
		Logout(playerID);
		});
}

void PlayerLifecycleSystem::Logout(Guid playerID)
{
	// TODO: Handle leave during login
	// TODO: Immediate logout on disconnect will be revisited later
	// TODO: Handle cases where player didn't enter any scene yet (e.g., login process or scene switch)
	LOG_INFO << "Logging out player: " << playerID;

	defer(tlsPlayerList.erase(playerID));

	const auto playerEntity = GetPlayer(playerID);
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_WARN << "Logout skipped, player entity invalid: " << playerID;
		return;
	}

	if (tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity))
	{
		LOG_DEBUG << "Player in scene, removing from scene: " << playerID;
		RoomCommon::LeaveRoom({ playerEntity });
	}

	GameNodeExitGameRequest exitGameRequest;
	SendMessageToPlayerOnSceneNode(ScenePlayerExitGameMessageId, exitGameRequest, playerEntity);

	Destroy(tlsRegistryManager.actorRegistry, playerEntity);
	LOG_INFO << "Destroyed player entity: " << playerID;
}
