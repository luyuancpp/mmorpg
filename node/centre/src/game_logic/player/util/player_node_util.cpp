#include "player_node_util.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "thread_local/storage.h"

#include "network/gate_session.h"
#include "game_logic/network/message_util.h"
#include "game_logic/player/comp/player_comp.h"
#include "service_info/game_player_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "game_logic/scene/util/player_change_scene_util.h"
#include "game_logic/scene/util/player_scene.h"
#include "scene/util/scene_util.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "util/defer.h"
#include "cpp_table_id_constants_name/globalvariable_table_id_constants.h"
#include "globalvariable_config.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"

void PlayerNodeUtil::HandlePlayerAsyncLoaded(Guid playerId, const player_centre_database& playerData)
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

	PlayerSceneUtil::HandleLoginEnterScene(playerEntity);
	// On database loaded
}

void PlayerNodeUtil::HandlePlayerAsyncSaved(Guid playerId, player_centre_database& playerData)
{
	// Placeholder for handling saved player data asynchronously
}

void PlayerNodeUtil::ProcessPlayerSessionState(entt::entity player)
{
	if (const auto* const enterGameFlag = tls.registry.try_get<EnterGameNodeInfoPBComponent>(player))
	{
		if (enterGameFlag->enter_gs_type() != LOGIN_NONE && enterGameFlag->enter_gs_type() != LOGIN_RECONNECT)
		{
			PlayerNodeUtil::HandlePlayerLogin(player);
		}
		else
		{
			PlayerNodeUtil::HandlePlayerReconnection(player);
		}

		tls.registry.remove<EnterGameNodeInfoPBComponent>(player);
	}
}

void PlayerNodeUtil::HandlePlayerLogin(entt::entity playerEntity)
{
	const auto enterGameFlag = tls.registry.try_get<EnterGameNodeInfoPBComponent>(playerEntity);
	if (!enterGameFlag)
	{
		return;
	}

	Centre2GsLoginRequest message;
	message.set_enter_gs_type(enterGameFlag->enter_gs_type());
	
	SendToGsPlayer(GamePlayerServiceCentre2GsLoginMessageId, message, playerEntity);
}

void PlayerNodeUtil::HandlePlayerReconnection(entt::entity player)
{

}

void PlayerNodeUtil::AddGameNodePlayerToGateNode(entt::entity playerEntity)
{
	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player session not found for player: " << tls.registry.try_get<Guid>(playerEntity);
		return;
	}

	entt::entity gateNodeId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate crash for session id: " << playerNodeInfo->gate_session_id();
		return;
	}

	auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateNodeId);
	if (!gateNode)
	{
		LOG_ERROR << "Gate crash for session id: " << playerNodeInfo->gate_session_id();
		return;
	}

	RegisterGameNodeSessionRequest request;
	request.mutable_session_info()->set_session_id(playerNodeInfo->gate_session_id());
	request.set_game_node_id(playerNodeInfo->game_node_id());
	(*gateNode)->CallRemoteMethod(GateServicePlayerEnterGameNodeMessageId, request);
}

void PlayerNodeUtil::HandleGameNodePlayerRegisteredAtGateNode(entt::entity playerEntity)
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
	SendToGs(GameServiceUpdateSessionDetailMessageId, request, playerNodeInfo->game_node_id());

	
}

void PlayerNodeUtil::HandleNormalExit(Guid playerID)
{
	Logout(playerID);
}

void PlayerNodeUtil::HandleAbnormalExit(Guid playerID)
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

void PlayerNodeUtil::Logout(Guid playerID)
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
