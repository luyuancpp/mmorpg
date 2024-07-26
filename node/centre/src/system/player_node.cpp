#include "player_node.h"
#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "thread_local/storage.h"

#include "network/gate_session.h"
#include "network/message_system.h"
#include "service/game_server_player_service.h"
#include "service/game_service_service.h"
#include "service/gate_service_service.h"
#include "system/player_change_scene.h"
#include "system/player_scene.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "util/defer.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_centre_database& playerData)
{
	auto& loadingList = tls.globalRegistry.get<PlayerLoadingInfoList>(global_entity());
	defer(loadingList.erase(playerId));
	const auto it = loadingList.find(playerId);
	if (it == loadingList.end())
	{
		LOG_ERROR << "Failed to load player: " << playerId;
		return;
	}

	auto playerEntity = tls.registry.create();

	if (const auto [first, success] = tls_cl.PlayerList().emplace(playerId, playerEntity); !success)
	{
		LOG_ERROR << "Error emplacing server: " << playerId;
		return;
	}

	tls.registry.emplace_or_replace<PlayerNodeInfo>(playerEntity).set_gate_session_id(it->second.session_info().session_id());
	tls.registry.emplace<Player>(playerEntity);
	tls.registry.emplace<Guid>(playerEntity, playerId);
	tls.registry.emplace<PlayerSceneInfoComp>(playerEntity, playerData.scene_info());

	PlayerChangeSceneSystem::InitChangeSceneQueue(playerEntity);

	// Set flag for first login
	tls.registry.emplace<EnterGsFlag>(playerEntity).set_enter_gs_type(LOGIN_FIRST);

	PlayerSceneSystem::OnLoginEnterScene(playerEntity);
	// On database loaded
}

void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, player_centre_database& playerData)
{
	// Placeholder for handling saved player data asynchronously
}

void PlayerNodeSystem::HandlePlayerLogin(entt::entity playerEntity)
{
	const auto enterGameFlag = tls.registry.try_get<EnterGsFlag>(playerEntity);
	if (!enterGameFlag)
	{
		return;
	}

	if (enterGameFlag->enter_gs_type() == LOGIN_FIRST)
	{
		// Handle first login scenario
	}
	else if (enterGameFlag->enter_gs_type() == LOGIN_REPLACE)
	{
		// Handle login replace scenario
	}
	else if (enterGameFlag->enter_gs_type() == LOGIN_RECONNECT)
	{
		// Handle reconnect scenario
	}

	{
		Centre2GsLoginRequest message;
		message.set_enter_gs_type(enterGameFlag->enter_gs_type());
		tls.registry.remove<EnterGsFlag>(playerEntity);
		SendToGsPlayer(GamePlayerServiceCentre2GsLoginMsgId, message, playerEntity);
	}
}

void PlayerNodeSystem::RegisterPlayerToGateNode(entt::entity playerEntity)
{
	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
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

	RegisterSessionGameNodeRequest request;
	request.mutable_session_info()->set_session_id(playerNodeInfo->gate_session_id());
	request.set_game_node_id(playerNodeInfo->game_node_id());
	(*gateNode)->CallMethod(GateServicePlayerEnterGsMsgId, request);
}

void PlayerNodeSystem::OnPlayerRegisteredToGateNode(entt::entity playerEntity)
{
	const auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
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
	SendToGs(GameServiceUpdateSessionMsgId, request, playerNodeInfo->game_node_id());

	if (const auto* const enterGameFlag = tls.registry.try_get<EnterGsFlag>(playerEntity))
	{
		if (enterGameFlag->enter_gs_type() != LOGIN_NONE)
		{
			HandlePlayerLogin(playerEntity);
		}
	}
}

void PlayerNodeSystem::HandlePlayerLeave(Guid playerUid)
{
	// TODO: Handle leave during login
	// TODO: Immediate logout on disconnect will be revisited later
	// TODO: Handle cases where player didn't enter any scene yet (e.g., login process or scene switch)
	defer(tls_cl.PlayerList().erase(playerUid));

	const auto playerEntity = tls_cl.get_player(playerUid);
	if (!tls.registry.valid(playerEntity))
	{
		return;
	}

	if (!tls.registry.try_get<SceneEntity>(playerEntity))
	{
		// Handle cases where player didn't enter any scene yet (e.g., login process or scene switch)
	}
	else
	{
		ScenesSystem::LeaveScene({ playerEntity });
	}
}
