#include "player_node_system.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "thread_local/storage.h"

#include "cpp_table_id_constants_name/globalvariable_table_id_constants.h"
#include "game_common_logic/system/session_system.h"
#include "network/message_system.h"
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
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "util/defer.h"
#include "proto/common/node.pb.h"
#include "cpp_table_id_constants_name/global_abnormal_logout_table_id_constants.h"
#include "globalvariable_config.h"

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_centre_database& playerData)
{
	auto& loadingList = tls.globalRegistry.get<PlayerLoadingInfoList>(GlobalEntity());
	defer(loadingList.erase(playerId));
	const auto it = loadingList.find(playerId);
	if (it == loadingList.end())
	{
		LOG_ERROR << "Failed to load player: " << playerId;
		return;
	}

	auto playerEntity = tls.registry.create();

	if (const auto [first, success] = tlsCommonLogic.GetPlayerList().emplace(playerId, playerEntity); !success)
	{
		LOG_ERROR << "Error emplacing server: " << playerId;
		return;
	}

	tls.registry.emplace_or_replace<PlayerNodeInfoPBComponent>(playerEntity).set_gate_session_id(it->second.session_info().session_id());
	tls.registry.emplace<Player>(playerEntity);
	tls.registry.emplace<Guid>(playerEntity, playerId);
	tls.registry.emplace<PlayerSceneContextPBComponent>(playerEntity, playerData.scene_info());

	PlayerChangeSceneUtil::InitChangeSceneQueue(playerEntity);

	// Set flag for first login
	tls.registry.emplace<EnterGameNodeInfoPBComponent>(playerEntity).set_enter_gs_type(LOGIN_FIRST);

	PlayerSceneSystem::HandleLoginEnterScene(playerEntity);
	// On database loaded
}

void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, player_centre_database& playerData)
{
	// Placeholder for handling saved player data asynchronously
}

void PlayerNodeSystem::ProcessPlayerSessionState(entt::entity player)
{
	if (const auto* const enterGameFlag = tls.registry.try_get<EnterGameNodeInfoPBComponent>(player))
	{
		if (enterGameFlag->enter_gs_type() != LOGIN_NONE && enterGameFlag->enter_gs_type() != LOGIN_RECONNECT)
		{
			PlayerNodeSystem::HandlePlayerLogin(player);
		}
		else
		{
			PlayerNodeSystem::HandlePlayerReconnection(player);
		}

		tls.registry.remove<EnterGameNodeInfoPBComponent>(player);
	}
}

void PlayerNodeSystem::HandlePlayerLogin(entt::entity playerEntity)
{
	const auto enterGameFlag = tls.registry.try_get<EnterGameNodeInfoPBComponent>(playerEntity);
	if (!enterGameFlag)
	{
		return;
	}

	Centre2GsLoginRequest message;
	message.set_enter_gs_type(enterGameFlag->enter_gs_type());
	
	SendToGsPlayer(ScenePlayerCentre2GsLoginMessageId, message, playerEntity);
}

void PlayerNodeSystem::HandlePlayerReconnection(entt::entity player)
{

}

void PlayerNodeSystem::AddGameNodePlayerToGateNode(entt::entity playerEntity)
{
	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
	if (!playerNodeInfo)
	{
		LOG_WARN << "Player session not found for player: " << tls.registry.try_get<Guid>(playerEntity);
		return;
	}

	entt::entity gateNodeId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
	if (!tls.GetNodeRegistry(eNodeType::GateNodeService).valid(gateNodeId))
	{
		LOG_WARN << "Gate crash for session id: " << playerNodeInfo->gate_session_id();
		return;
	}

	auto gateNodeScene = tls.GetNodeRegistry(eNodeType::GateNodeService).try_get<RpcSession>(gateNodeId);
	if (!gateNodeScene)
	{
		LOG_WARN << "Gate crash for session id: " << playerNodeInfo->gate_session_id();
		return;
	}

	RegisterGameNodeSessionRequest request;
	request.mutable_session_info()->set_session_id(playerNodeInfo->gate_session_id());
	request.set_scene_node_id(playerNodeInfo->scene_node_id());
	gateNodeScene->CallRemoteMethod(GatePlayerEnterGameNodeMessageId, request);
}

void PlayerNodeSystem::HandleGameNodePlayerRegisteredAtGateNode(entt::entity playerEntity)
{
	const auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Invalid player session";
		return;
	}

	const auto* const playerId = tls.registry.try_get<Guid>(playerEntity);
	if (!playerId)
	{
		LOG_ERROR << "Player not found";
		return;
	}

	RegisterPlayerSessionRequest request;
	request.set_session_id(playerNodeInfo->gate_session_id());
	request.set_player_id(*playerId);
	SendToGs(SceneUpdateSessionDetailMessageId, request, playerNodeInfo->scene_node_id());

	
}

void PlayerNodeSystem::HandleNormalExit(Guid playerID)
{
	Logout(playerID);
}

void PlayerNodeSystem::HandleAbnormalExit(Guid playerID)
{
	const auto playerEntity = tlsCommonLogic.GetPlayer(playerID);
	if (!tls.registry.valid(playerEntity))
	{
		LOG_ERROR << "Player not found: " << playerID;
		return;
	}

	FetchGlobalVariableTableOrReturnVoid(kGlobalVariable_Abnormal_logout);

	tls.registry.emplace_or_replace<AbnormalExitTimer>(playerEntity).timer.RunAfter(globalVariableTable->todouble(), [playerID]() {Logout(playerID); });
}

void PlayerNodeSystem::Logout(Guid playerID)
{
	// TODO: Handle leave during login
	// TODO: Immediate logout on disconnect will be revisited later
	// TODO: Handle cases where player didn't enter any scene yet (e.g., login process or scene switch)
	defer(tlsCommonLogic.GetPlayerList().erase(playerID));

	const auto playerEntity = tlsCommonLogic.GetPlayer(playerID);
	if (!tls.registry.valid(playerEntity))
	{
		return;
	}

	if (tls.registry.try_get<SceneEntityComp>(playerEntity))
	{
		SceneUtil::LeaveScene({ playerEntity });
	}
	
}
