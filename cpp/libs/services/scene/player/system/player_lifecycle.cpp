#include "player_lifecycle.h"
#include "proto/common/event/actor_event.pb.h"
#include "proto/common/component/player_async_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "proto/common/component/player_login_comp.pb.h"
#include "proto/common/component/player_network_comp.pb.h"
#include "proto/common/event/player_event.pb.h"

#include "thread_context/redis_manager.h"
#include "time/system/time.h"
#include "type_alias/player_session_type_alias.h"
#include "core/utils/defer/defer.h"
#include "network/node_utils.h"
#include "player/system/player_data_loader.h"
#include "engine/core/type_define/type_define.h"
#include "proto/common/event/player_migration_event.pb.h"
#include "node/system/zone_utils.h"
#include "table/proto/tip/cross_server_error_tip.pb.h"
#include "player_tip.h"
#include "modules/scene/comp/scene_comp.h"
#include <engine/infra/messaging/kafka/kafka_producer.h>
#include "thread_context/player_manager.h"
#include "core/system/redis.h"
#include "thread_context/dispatcher_manager.h"
#include "player_scene.h"

struct PlayerSceneEnterContext
{
	PlayerGameNodeEntryInfoComp enterInfo;
	uint64_t sceneId{ 0 };
};


void PlayerLifecycleSystem::HandlePlayerAsyncLoaded(Guid playerId, const PlayerAllData& message, const std::any& extra)
{
	LOG_INFO << "HandlePlayerAsyncLoaded: Loading player " << playerId;
	
	if (extra.type() == typeid(PlayerSceneEnterContext))
	{
		const auto& context = std::any_cast<PlayerSceneEnterContext>(extra);
		auto player = InitPlayerFromAllData(message, context.enterInfo);
		if (tlsRegistryManager.actorRegistry.valid(player))
		{
			PlayerSceneSystem::HandleEnterScene(player, entt::to_entity(context.sceneId));
		}
	}
	else if (extra.type() == typeid(PlayerGameNodeEntryInfoComp))
	{
		const auto& enterInfo = std::any_cast<PlayerGameNodeEntryInfoComp>(extra);
		InitPlayerFromAllData(message, enterInfo);
	}
	else
	{
		LOG_ERROR << "HandlePlayerAsyncLoaded: Invalid extra data type for player " << playerId;
	}
}

void PlayerLifecycleSystem::HandlePlayerAsyncSaved(Guid playerId, PlayerAllData& message)
{
	LOG_INFO << "HandlePlayerAsyncSaved: Saving complete for player: " << playerId;

	// TODO: When should session be deleted?

	auto playerEntity = GetPlayer(playerId);
	HandleCrossZoneTransfer(playerEntity);

	if (tlsRegistryManager.actorRegistry.any_of<UnregisterPlayer>(playerEntity))
	{
		LOG_INFO << "Player marked for unregistration: " << playerId;

		// Must complete save before deleting session; consider better ordering
		RemovePlayerSession(playerId);
		LOG_INFO << "Player session removed";
		// TODO: Verify no race condition on destroy-after-save ordering
		DestroyPlayer(playerId);
	}

	// Centre decommissioned: save-complete notification no longer needed.
	// player_locator lease handles reconnect gating via Redis TTL.
}

// CONSIDER: handle reentry into a different scene node while load is still in progress
void PlayerLifecycleSystem::EnterScene(const entt::entity player, const PlayerGameNodeEntryInfoComp& enterInfo){
	LOG_INFO << "EnterScene: Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " entering scene node";

	// Centre decommissioned: no longer track centre_node_id or send CentreEnterGsSucceed.
	// player_locator (Go service) owns session/location truth via Redis.
	// TODO: After Centre restarts
	// TODO: After scene node updates its gate, send to client only when ready.
	//   Message ordering concern: enter scene_node_a then enter scene_node_b —
	//   leave-scene message from node_a may arrive after enter-scene from node_b.
	//   Scene node should wait for gate's leave-ack before broadcasting enter events.
}

void PlayerLifecycleSystem::OnPlayerLogin(entt::entity player, uint32_t enterGsType)
{
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "OnPlayerLogin: invalid player entity";
		return;
	}

	const auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
	auto& enterState = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerEnterGameStateComp>(player);
	enterState.set_enter_gs_type(enterGsType);

	auto* sceneEntity = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(player);
	if (sceneEntity == nullptr || !tlsRegistryManager.sceneRegistry.valid(sceneEntity->sceneEntity))
	{
		LOG_WARN << "OnPlayerLogin: player has no valid scene binding yet, player=" << playerId
			<< " enter_gs_type=" << enterGsType;
		return;
	}

	switch (enterGsType)
	{
	case LOGIN_FIRST:
	case LOGIN_REPLACE:
	case LOGIN_RECONNECT:
		LOG_INFO << "OnPlayerLogin: enter_gs_type=" << enterGsType << " player=" << playerId;
		PlayerSceneSystem::HandleEnterScene(player, sceneEntity->sceneEntity);
		break;
	case LOGIN_NONE:
		LOG_DEBUG << "OnPlayerLogin: LOGIN_NONE ignored, player=" << playerId;
		break;
	default:
		LOG_ERROR << "OnPlayerLogin: unknown login type=" << enterGsType << " player=" << playerId;
		break;
	}
}

void PlayerLifecycleSystem::HandleBindPlayerToGateOK(entt::entity player)
{
	// TODO: notify client that gate binding is ready; send initial scene state snapshot
}

// TODO: Validate session before removal
void PlayerLifecycleSystem::RemovePlayerSession(const Guid playerId)
{
	auto playerIt = tlsPlayerList.find(playerId);
	if (playerIt == tlsPlayerList.end())
	{
		LOG_ERROR << "RemovePlayerSession: player entity not found in session map for player: " << playerId;
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerLifecycleSystem::RemovePlayerSession(entt::entity player)
{
	auto* const playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (playerSessionSnapshotPB == nullptr)
	{
		LOG_ERROR << "RemovePlayerSession: PlayerSessionSnapshotComp not found for player: " << entt::to_integral(player);
		return;
	}

	LOG_INFO << "Removing player session: sessionId = " << playerSessionSnapshotPB->gate_session_id();

	defer(SessionMap().erase(playerSessionSnapshotPB->gate_session_id()));
	playerSessionSnapshotPB->set_gate_session_id(kInvalidSessionId);
}

void PlayerLifecycleSystem::RemovePlayerSessionSilently(Guid playerId)
{
	auto playerIt = tlsPlayerList.find(playerId);
	if (playerIt == tlsPlayerList.end())
	{
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerLifecycleSystem::DestroyPlayer(Guid playerId)
{
	LOG_INFO << "Destroying player: " << playerId;

	defer(tlsPlayerList.erase(playerId));
	DestroyEntity(tlsRegistryManager.actorRegistry, GetPlayer(playerId));
}

void PlayerLifecycleSystem::HandleExitGameNode(entt::entity player)
{
	LOG_INFO << "HandleExitGameNode: Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " is exiting the scene node";

	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "HandleExitGameNode: Player entity is not valid";
		return;
	}

	if (tlsRegistryManager.actorRegistry.all_of<UnregisterPlayer>(player))
	{
		LOG_INFO << "Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " is already marked for unregistration";
		return;
	}

	tlsRegistryManager.actorRegistry.emplace<UnregisterPlayer>(player);

	PlayerLifecycleSystem::SavePlayerToRedis(player);

	// TODO: Only allow re-login after save completes

}

void PlayerLifecycleSystem::HandleCrossZoneTransfer(entt::entity playerEntity)
{
	auto changeInfo = tlsRegistryManager.actorRegistry.try_get<ChangeSceneInfoComp>(playerEntity);
	if (!changeInfo)
	{
		return;
	}

	if (!changeInfo->is_cross_zone())
	{
		return;
	}
	
	auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);

	PlayerAllData playerAllDataMessage;
	PlayerAllDataMessageFieldsMarshal(playerEntity, playerAllDataMessage);
	playerAllDataMessage.mutable_player_database_data()->set_player_id(playerId);
	playerAllDataMessage.mutable_player_database_1_data()->set_player_id(playerId);

	PlayerMigrationPbEvent request;
	request.set_player_id(playerId);
	request.set_from_zone(GetZoneId());
	request.set_to_zone(changeInfo->to_zone_id());
	request.mutable_scene_info()->CopyFrom(*changeInfo);
	request.set_serialized_player_data(std::move(playerAllDataMessage.SerializeAsString()));
	// Centre decommissioned: centre_node_id no longer carried in migration messages.

	KafkaProducer::Instance().send("player_migrate", request.SerializeAsString(), std::to_string(playerId), changeInfo->to_zone_id());

	LOG_INFO << "[CrossZone] Sent player transfer to zone " << changeInfo->to_zone_id() << ": " << playerId;

	PlayerTipSystem::SendToPlayer(playerEntity, kSceneTransferInProgress, {});

	tlsRegistryManager.actorRegistry.remove<ChangeSceneInfoComp>(playerEntity);
}

void PlayerLifecycleSystem::HandlePlayerMigration(const PlayerMigrationPbEvent& msg) {
	PlayerAllData playerAllDataMessage;
	if (!playerAllDataMessage.ParseFromString(msg.serialized_player_data())) {
		LOG_ERROR << "Parse failed for player migration data";
		return;
	}

	PlayerGameNodeEntryInfoComp enterInfo;
	// Centre decommissioned: centre_node_id no longer piggybacked in migration.

	auto player = InitPlayerFromAllData(playerAllDataMessage, enterInfo);
	SavePlayerToRedis(player);
}

entt::entity PlayerLifecycleSystem::InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEntryInfoComp& enterInfo)
{
	auto playerId = playerAllData.player_database_data().player_id();

	LOG_INFO << "[InitPlayerFromAllData] Init player: " << playerId;

	auto player = tlsRegistryManager.actorRegistry.create();

	// Register in global player-entity map
	if (const auto [it, inserted] = tlsPlayerList.emplace(playerId, player); !inserted)
	{
		LOG_ERROR << "[InitPlayerFromAllData] Player already exists in GlobalPlayerList: " << playerId;
		return entt::null;
	}

	tlsRegistryManager.actorRegistry.emplace<Player>(player);
	tlsRegistryManager.actorRegistry.emplace<Guid>(player, playerId);

	PlayerAllDataMessageFieldsUnMarshal(player, playerAllData);

	// First-time registration: initialize defaults
	if (playerAllData.player_database_data().uint64_pb_component().registration_timestamp() <= 0)
	{
		tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint64Comp>(player).set_registration_timestamp(TimeSystem::NowSecondsUTC());
		tlsRegistryManager.actorRegistry.get_or_emplace<LevelComp>(player).set_level(1);

		RegisterPlayerEvent registerPlayer;
		registerPlayer.set_actor_entity(entt::to_integral(player));
		dispatcher.trigger(registerPlayer);
	}

	tlsRegistryManager.actorRegistry.emplace<ViewRadius>(player).set_radius(10);

	// Centre decommissioned: centre_node_id no longer tracked in session snapshot.
	// player_locator (Go) owns the canonical session/location record.

	// Fire component initialization events
	InitializeActorComponentsEvent initActorEvent;
	initActorEvent.set_actor_entity(entt::to_integral(player));
	dispatcher.trigger(initActorEvent);

	InitializePlayerComponentsEvent initPlayerEvent;
	initPlayerEvent.set_actor_entity(entt::to_integral(player));
	dispatcher.trigger(initPlayerEvent);

	EnterScene(player, enterInfo);

	return player;
}

void PlayerLifecycleSystem::SavePlayerToRedis(entt::entity player)
{
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "[SavePlayerToRedis] Invalid player entity";
		return;
	}

	auto playerId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	using SaveMessage = PlayerDataRedis::element_type::MessageValuePtr;
	SaveMessage message = std::make_shared<SaveMessage::element_type>();

	PlayerAllDataMessageFieldsMarshal(player, *message);

	tlsRedisSystem.GetPlayerDataRedis()->Save(message, playerId);

	LOG_INFO << "[SavePlayerToRedis] Player " << playerId << " saved to Redis";
}

