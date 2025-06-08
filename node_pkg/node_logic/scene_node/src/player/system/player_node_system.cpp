#include "player_node_system.h"

#include "node/scene_node_info.h"
#include "proto/logic/event/actor_event.pb.h"
#include "core/network/message_system.h"
#include "proto/centre/centre_service.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/event/player_event.pb.h"
#include "service_info/centre_player_scene_service_info.h"
#include "service_info/centre_service_service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "time/system/time_system.h"
#include "type_alias/player_session_type_alias.h"
#include "util/defer.h"

void Player_databaseMessageFieldsUnmarshal(entt::entity player, const player_database& message);
void Player_databaseMessageFieldsMarshal(entt::entity player, player_database& message);

void Player_database_1MessageFieldsUnmarshal(entt::entity player, const player_database& message);
void Player_database_1MessageFieldsMarshal(entt::entity player, player_database& message);

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_database& message)
{
	LOG_INFO << "Player loaded: " << playerId;

	const auto asyncIt = tlsGame.playerNodeEntryInfoList.find(playerId);
	if (asyncIt == tlsGame.playerNodeEntryInfoList.end())
	{
		LOG_ERROR << "Async player not found: " << playerId;
		return;
	}

	defer(tlsGame.playerNodeEntryInfoList.erase(playerId));

	auto player = tls.registry.create();
	if (const auto [first, second] = tlsCommonLogic.GetPlayerList().emplace(playerId, player); !second)
	{
		LOG_ERROR << "Failed to emplace player: " << playerId;
		return;
	}

	// Populate player data from database message
	tls.registry.emplace<Player>(player);
	tls.registry.emplace<Guid>(player, message.player_id());
	Player_databaseMessageFieldsUnmarshal(player, message);

	if (message.uint64_pb_component().registration_timestamp() <= 0)
	{
		tls.registry.get<PlayerUint64PBComponent>(player).set_registration_timestamp(TimeUtil::NowSecondsUTC());

		tls.registry.get<LevelPbComponent>(player).set_level(1);
		
		RegisterPlayerEvent registerPlayer;
		registerPlayer.set_actor_entity(entt::to_integral(player));
		tls.dispatcher.trigger(registerPlayer);
	}

	tls.registry.emplace<ViewRadius>(player).set_radius(10);
	tls.registry.emplace<PlayerNodeInfoPBComponent>(player).set_centre_node_id(asyncIt->second.centre_node_id());

	InitializeActorComponentsEvent initializeActorComponentsEvent;
	initializeActorComponentsEvent.set_actor_entity(entt::to_integral(player));
	tls.dispatcher.trigger(initializeActorComponentsEvent);
	
	InitializePlayerComponentsEvent initializePlayerComponents;
	initializePlayerComponents.set_actor_entity(entt::to_integral(player));
	tls.dispatcher.trigger(initializePlayerComponents);
	
	// Notify game node about player entering
	EnterGs(player, asyncIt->second);
}


void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, player_database& message)
{
	//todo session 啥时候删除？
	//告诉Centre 保存完毕，可以切换场景了,或者再登录可以重新上线了
	CentreLeaveSceneAsyncSavePlayerCompleteRequest request;
	SendToCentrePlayerById(CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId, request, playerId);

	if (tls.registry.any_of<UnregisterPlayer>(tlsCommonLogic.GetPlayer(playerId)))
	{
		//存储完毕之后才删除,有没有更好办法做到先删除session 再存储
		RemovePlayerSession(playerId);
		//todo 会不会有问题
		//存储完毕从gs删除玩家
		DestroyPlayer(playerId);
	}
}

void PlayerNodeSystem::SavePlayer(entt::entity player)
{
	using SaveMessage = PlayerRedis::element_type::MessageValuePtr;
	SaveMessage pb = std::make_shared<SaveMessage::element_type>();

	pb->set_player_id(tls.registry.get<Guid>(player));
	Player_databaseMessageFieldsMarshal(player, *pb);
	tlsGame.playerRedis->Save(pb, tls.registry.get<Guid>(player));
}

//考虑: 没load 完再次进入别的gs
void PlayerNodeSystem::EnterGs(const entt::entity player, const PlayerGameNodeEnteryInfoPBComponent& enterInfo)
{
	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
	if (playerNodeInfo == nullptr)
	{
		LOG_ERROR << "Player node info not found for player: " << tls.registry.get<Guid>(player);
		playerNodeInfo = &tls.registry.emplace<PlayerNodeInfoPBComponent>(player);
	}

	playerNodeInfo->set_centre_node_id(enterInfo.centre_node_id());

	// Notify Centre that player has entered the game node successfully
	NotifyEnterGsSucceed(player, enterInfo.centre_node_id());
	//todo Centre 重新启动以后
	//todo gs更新了对应的gate之后 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，
	//进入game_node a, 再进入game_node b 两个gs的消息到达客户端消息的顺序不一样,所以说game 还要通知game 还要收到gate 的处理完准备离开game的消息
	//否则两个不同的gs可能离开场景的消息后于进入场景的消息到达客户端
}

void PlayerNodeSystem::NotifyEnterGsSucceed(entt::entity player, NodeId centreNodeId)
{
	EnterGameNodeSuccessRequest request;
	request.set_player_id(tls.registry.get<Guid>(player));
	request.set_scene_node_id(GetNodeInfo().node_id());
	CallCentreNodeMethod(CentreEnterGsSucceedMessageId, request, centreNodeId);

	// TODO: Handle game node update corresponding to gate before sending client messages
	// Example: Ensure gate updates are done before client messages can be sent
	// This ensures that the message order received by clients is consistent
}

void PlayerNodeSystem::LeaveGs(entt::entity player)
{
}

void PlayerNodeSystem::OnPlayerLogin(entt::entity player, uint32_t enterGsType)
{
	switch (enterGsType)
	{
	case LOGIN_FIRST:
		// First time login logic
		break;
	case LOGIN_REPLACE:
		// Replace login logic (e.g., when another session is active)
		break;
	case LOGIN_RECONNECT:
		// Reconnect logic (e.g., rejoining after disconnect)
		break;
	default:
		LOG_ERROR << "Unknown login type: " << enterGsType;
		break;
	}
}

void PlayerNodeSystem::HandleGameNodePlayerRegisteredAtGateNode(entt::entity player)
{

}

//todo 检测
void PlayerNodeSystem::RemovePlayerSession(const Guid playerId)
{
	auto playerIt = tlsCommonLogic.GetPlayerList().find(playerId);
	if (playerIt == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerNodeSystem::RemovePlayerSession(entt::entity player)
{
	auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
	if (playerNodeInfo == nullptr)
	{
		LOG_ERROR << "Player node info not found";
		return;
	}

	defer(tlsSessions.erase(playerNodeInfo->gate_session_id()));
	playerNodeInfo->set_gate_session_id(kInvalidSessionId);
}

void PlayerNodeSystem::RemovePlayerSessionSilently(Guid player_id)
{
	auto playerIt = tlsCommonLogic.GetPlayerList().find(player_id);
	if (playerIt == tlsCommonLogic.GetPlayerList().end())
	{
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerNodeSystem::DestroyPlayer(Guid playerId)
{
	defer(tlsCommonLogic.GetPlayerList().erase(playerId));
	Destroy(tls.registry, tlsCommonLogic.GetPlayer(playerId));
}

void PlayerNodeSystem::HandleExitGameNode(entt::entity player)
{
	// 离开gs 清除session
	PlayerNodeSystem::SavePlayer(player);
	tls.registry.emplace<UnregisterPlayer>(player);
	//todo 存完之后center 才能再次登录
}
