#include "player_node_system.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "thread_local/storage.h"
#include "game_common_logic/system/session_system.h"
#include "player/comp/player_comp.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/player_scene_system.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "scene/system/scene_system.h"
#include "service_info/game_player_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "thread_local/storage_common_logic.h"
#include "util/defer.h"
#include "proto/common/node.pb.h"
#include "cpp_table_id_constants_name/global_abnormal_logout_table_id_constants.h"
#include "globalvariable_config.h"
#include "proto/logic/component/player_scene_comp.pb.h"
#include "proto/db/mysql_database_table.pb.h"
#include "network/network_utils.h"
#include "network/node_message_utils.h"
#include "network/rpc_session.h"
#include "network/player_message_utils.h"
#include "type_alias/player_session_type_alias.h"
#include "thread_local/thread_local_node_context.h"
#include "thread_local/player_storage.h"

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_centre_database& playerData, const std::any& extra)
{
	//load 回来之前断开连接了,然后又加到redis了 这种怎么办,session id 变了咋办

	LOG_INFO << "Handling async load for player: " << playerId;
	assert(gPlayerList.find(playerId) == gPlayerList.end());

	auto sessionPbComp = std::any_cast<PlayerSessionSnapshotPBComp>(extra);

	if (GlobalSessionList().find(sessionPbComp.gate_session_id()) == GlobalSessionList().end())
	{
		return;
	}
	
	auto playerEntity = tls.actorRegistry.create();
	if (const auto [first, success] = gPlayerList.emplace(playerId, playerEntity); !success)
	{
		LOG_ERROR << "Error emplacing player in player list: " << playerId;
		return;
	}

	auto& sessionPB = tls.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(playerEntity, std::move(sessionPbComp));

	tls.actorRegistry.emplace<Player>(playerEntity);
	tls.actorRegistry.emplace<Guid>(playerEntity, playerId);
	tls.actorRegistry.emplace<PlayerSceneContextPBComponent>(playerEntity, playerData.scene_info());

	tls.actorRegistry.emplace<PlayerEnterGameStatePbComp>(playerEntity).set_enter_gs_type(LOGIN_FIRST);

	PlayerSceneSystem::HandleLoginEnterScene(playerEntity);
	LOG_INFO << "Player login enter scene handled: " << playerId;
}

void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, player_centre_database& playerData)
{
	LOG_INFO << "Handling async save for player: " << playerId;
	// Placeholder for handling saved player data asynchronously
}

void PlayerNodeSystem::ProcessPlayerSessionState(entt::entity player)
{
	if (const auto* const enterGameFlag = tls.actorRegistry.try_get<PlayerEnterGameStatePbComp>(player))
	{
		LOG_DEBUG << "EnterGameNodeInfoPBComponent found with type: " << enterGameFlag->enter_gs_type();

		if (enterGameFlag->enter_gs_type() != LOGIN_NONE && enterGameFlag->enter_gs_type() != LOGIN_RECONNECT)
		{
			PlayerNodeSystem::HandlePlayerLogin(player);
		}
		else
		{
			PlayerNodeSystem::HandlePlayerReconnection(player);
		}

		tls.actorRegistry.remove<PlayerEnterGameStatePbComp>(player);
		LOG_DEBUG << "Removed EnterGameNodeInfoPBComponent from player";
	}
}

void PlayerNodeSystem::HandlePlayerLogin(entt::entity playerEntity)
{
	const auto enterGameFlag = tls.actorRegistry.try_get<PlayerEnterGameStatePbComp>(playerEntity);
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

void PlayerNodeSystem::HandlePlayerReconnection(entt::entity player)
{
	LOG_INFO << "Handling player reconnection for entity: " << static_cast<uint32_t>(player);
}

void PlayerNodeSystem::AddGameNodePlayerToGateNode(entt::entity playerEntity)
{
	auto* sessionPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB)
	{
		LOG_WARN << "PlayerSessionSnapshotPB not found for player: " << tls.actorRegistry.try_get<Guid>(playerEntity);
		return;
	}

	LOG_INFO << "Adding game node player to gate node, session_id: " << sessionPB->gate_session_id();

	entt::entity gateNodeId{ GetGateNodeId(sessionPB->gate_session_id()) };
	auto& registry = ThreadLocalNodeContext::Instance().GetRegistry(eNodeType::GateNodeService);
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

void PlayerNodeSystem::HandleSceneNodePlayerRegisteredAtGateNode(entt::entity playerEntity)
{
	const auto* const sessionPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB)
	{
		LOG_ERROR << "Invalid player session in HandleGameNodePlayerRegisteredAtGateNode";
		return;
	}
	const auto* const playerId = tls.actorRegistry.try_get<Guid>(playerEntity);
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

void PlayerNodeSystem::HandleNormalExit(Guid playerID)
{
	LOG_INFO << "Handling normal exit for player: " << playerID;
	Logout(playerID);
}

void PlayerNodeSystem::HandleAbnormalExit(Guid playerID)
{
	LOG_INFO << "Handling abnormal exit for player: " << playerID;

	const auto playerEntity = PlayerManager::Instance().GetPlayer(playerID);
	if (!tls.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity not valid for abnormal exit: " << playerID;
		return;
	}

	FetchGlobalVariableTableOrReturnVoid(kGlobalVariable_Abnormal_logout);

	LOG_DEBUG << "Starting abnormal logout timer for player: " << playerID;
	tls.actorRegistry.get_or_emplace<AbnormalExitTimer>(playerEntity).timer.RunAfter(globalVariableTable->todouble(), [playerID]() {
		LOG_INFO << "Abnormal exit timeout reached for player: " << playerID;
		Logout(playerID);
		});
}

void PlayerNodeSystem::Logout(Guid playerID)
{
	// TODO: Handle leave during login
	// TODO: Immediate logout on disconnect will be revisited later
	// TODO: Handle cases where player didn't enter any scene yet (e.g., login process or scene switch)
	LOG_INFO << "Logging out player: " << playerID;

	defer(gPlayerList.erase(playerID));

	const auto playerEntity = PlayerManager::Instance().GetPlayer(playerID);
	if (!tls.actorRegistry.valid(playerEntity))
	{
		LOG_WARN << "Logout skipped, player entity invalid: " << playerID;
		return;
	}

	if (tls.actorRegistry.try_get<SceneEntityComp>(playerEntity))
	{
		LOG_DEBUG << "Player in scene, removing from scene: " << playerID;
		SceneUtil::LeaveScene({ playerEntity });
	}

	GameNodeExitGameRequest exitGameRequest;
	SendMessageToPlayerOnSceneNode(ScenePlayerExitGameMessageId, exitGameRequest, playerEntity);

	Destroy(tls.actorRegistry, playerEntity);
	LOG_INFO << "Destroyed player entity: " << playerID;
}
