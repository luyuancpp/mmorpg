#include "player_node_system.h"
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

#include "threading/redis_manager.h"
#include "time/system/time_system.h"
#include "type_alias/player_session_type_alias.h"
#include "util/defer.h"
#include "network/node_utils.h"
#include "network/node_message_utils.h"
#include "player/system/player_data_loader_system.h"
#include "base/core/type_define/type_define.h"
#include "proto/db/mysql_database_table.pb.h"
#include "proto/logic/event/player_migration_event.pb.h"
#include "node/system/zone_utils.h"
#include "table/proto/tip/cross_server_error_tip.pb.h"
#include "player_tip_system.h"
#include <base/infra/messaging/kafka/kafka_producer.h>
#include "threading/player_manager.h"
#include "core/system/redis_system.h"
#include <threading/dispatcher_manager.h>

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const PlayerAllData& message, const std::any& extra)
{
	LOG_INFO << "HandlePlayerAsyncLoaded: Loading player " << playerId;
	auto enterInfo = std::any_cast<PlayerGameNodeEnteryInfoPBComponent>(extra);
	InitPlayerFromAllData(message, enterInfo);
}

void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, PlayerAllData& message)
{
	LOG_INFO << "HandlePlayerAsyncSaved: Saving complete for player: " << playerId;

	//todo session 啥时候删除？

	auto playerEntity = GetPlayer(playerId);
	HandleCrossZoneTransfer(playerEntity);

	if (tlsRegistryManager.actorRegistry.any_of<UnregisterPlayer>(playerEntity))
	{
		LOG_INFO << "Player marked for unregistration: " << playerId;

		//存储完毕之后才删除,有没有更好办法做到先删除session 再存储
		RemovePlayerSession(playerId);
		LOG_INFO << "Player session removed";
		//todo 会不会有问题
		//存储完毕从gs删除玩家
		DestroyPlayer(playerId);
	}

	//告诉Centre 保存完毕，可以切换场景了,或者再登录可以重新上线了
	CentreLeaveSceneAsyncSavePlayerCompleteRequest request;
	SendToCentrePlayerByClientNode(CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId, request, playerId);
}

//考虑: 没load 完再次进入别的gs
void PlayerNodeSystem::EnterGs(const entt::entity player, const PlayerGameNodeEnteryInfoPBComponent& enterInfo){
	LOG_INFO << "EnterGs: Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " entering Game Node";

	auto& playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(player);
	auto& nodeIdMap = *playerSessionSnapshotPB.mutable_node_id();
	nodeIdMap[eNodeType::CentreNodeService] = enterInfo.centre_node_id();
	LOG_INFO << "Updated PlayerNodeInfo with CentreNodeId: " << enterInfo.centre_node_id();

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
	request.set_player_id(tlsRegistryManager.actorRegistry.get<Guid>(player));
	request.set_scene_node_id(GetNodeInfo().node_id());
	CallRemoteMethodOnClient(CentreEnterGsSucceedMessageId, request, centreNodeId, eNodeType::CentreNodeService);

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

void PlayerNodeSystem::HandleSceneNodePlayerRegisteredAtGateNode(entt::entity player)
{

}

//todo 检测
void PlayerNodeSystem::RemovePlayerSession(const Guid playerId)
{
	auto playerIt = tlsPlayerList.find(playerId);
	if (playerIt == tlsPlayerList.end())
	{
		LOG_ERROR << "RemovePlayerSession: PlayerNodeInfoPBComponent not found for player: " << playerId;
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerNodeSystem::RemovePlayerSession(entt::entity player)
{
	auto* const playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
	if (playerSessionSnapshotPB == nullptr)
	{
		LOG_ERROR << "RemovePlayerSession: PlayerNodeInfoPBComponent not found for player: " << entt::to_integral(player);
		return;
	}

	LOG_INFO << "Removing player session: sessionId = " << playerSessionSnapshotPB->gate_session_id();

	defer(GlobalSessionList().erase(playerSessionSnapshotPB->gate_session_id()));
	playerSessionSnapshotPB->set_gate_session_id(kInvalidSessionId);
}

void PlayerNodeSystem::RemovePlayerSessionSilently(Guid player_id)
{
	auto playerIt = tlsPlayerList.find(player_id);
	if (playerIt == tlsPlayerList.end())
	{
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerNodeSystem::DestroyPlayer(Guid playerId)
{
	LOG_INFO << "Destroying player: " << playerId;

	defer(tlsPlayerList.erase(playerId));
	Destroy(tlsRegistryManager.actorRegistry, GetPlayer(playerId));
}

void PlayerNodeSystem::HandleExitGameNode(entt::entity player)
{
	LOG_INFO << "HandleExitGameNode: Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " is exiting the Game Node";

	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "HandleExitGameNode: Player entity is not valid";
		return;
	}

	if (tlsRegistryManager.actorRegistry.all_of<UnregisterPlayer>(player))
	{
		LOG_INFO << "Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " is already marked for unregistration";
		return;
	}

	tlsRegistryManager.actorRegistry.emplace<UnregisterPlayer>(player);

	PlayerNodeSystem::SavePlayerToRedis(player);

	//todo 存完之后center 才能再次登录

}

void PlayerNodeSystem::HandleCrossZoneTransfer(entt::entity playerEntity)
{
	auto changeInfo = tlsRegistryManager.actorRegistry.try_get<ChangeSceneInfoPBComponent>(playerEntity);
	if (!changeInfo)
	{
		return;
	}

	if (!changeInfo->is_cross_zone())
	{
		return;
	}
	
	auto playerId = tlsRegistryManager.actorRegistry.get<Guid>(playerEntity);

	auto& sessionSnapshot = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(playerEntity);
	auto& nodeIdMap = *sessionSnapshot.mutable_node_id();

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
	request.set_centre_node_id(nodeIdMap[eNodeType::CentreNodeService]);

	KafkaProducer::Instance().send("player_migrate", request.SerializeAsString(), std::to_string(playerId), changeInfo->to_zone_id());

	LOG_INFO << "[CrossZone] Sent player transfer to zone " << changeInfo->to_zone_id() << ": " << playerId;

	PlayerTipSystem::SendToPlayer(playerEntity, kSceneTransferInProgress, {});

	tlsRegistryManager.actorRegistry.remove<ChangeSceneInfoPBComponent>(playerEntity);
}

void PlayerNodeSystem::HandlePlayerMigration(const PlayerMigrationPbEvent& msg) {
	PlayerAllData playerAllDataMessage;
	if (!playerAllDataMessage.ParseFromString(msg.serialized_player_data())) {
		LOG_ERROR << "Parse failed for player migration data";
		return;
	}

	PlayerGameNodeEnteryInfoPBComponent enterInfo; // 已在消息中带上
	enterInfo.set_centre_node_id(msg.centre_node_id());

	auto player = InitPlayerFromAllData(playerAllDataMessage, enterInfo);
	SavePlayerToRedis(player);
}

entt::entity PlayerNodeSystem::InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEnteryInfoPBComponent& enterInfo)
{
	auto playerId = playerAllData.player_database_data().player_id();

	LOG_INFO << "[InitPlayerFromAllData] Init player: " << playerId;

	// 1. 创建实体
	auto player = tlsRegistryManager.actorRegistry.create();

	// 2. 注册全局玩家实体映射
	if (const auto [it, inserted] = tlsPlayerList.emplace(playerId, player); !inserted)
	{
		LOG_ERROR << "[InitPlayerFromAllData] Player already exists in GlobalPlayerList: " << playerId;
		return entt::null;
	}

	// 3. 设置基本组件
	tlsRegistryManager.actorRegistry.emplace<Player>(player);
	tlsRegistryManager.actorRegistry.emplace<Guid>(player, playerId);

	// 4. 反序列化全量数据
	PlayerAllDataMessageFieldsUnMarshal(player, playerAllData);

	// 5. 初始化必要数据（仅首次注册时）
	if (playerAllData.player_database_data().uint64_pb_component().registration_timestamp() <= 0)
	{
		tlsRegistryManager.actorRegistry.get<PlayerUint64PBComponent>(player).set_registration_timestamp(TimeUtil::NowSecondsUTC());
		tlsRegistryManager.actorRegistry.get<LevelPbComponent>(player).set_level(1);

		RegisterPlayerEvent registerPlayer;
		registerPlayer.set_actor_entity(entt::to_integral(player));
		dispatcher.trigger(registerPlayer);
	}

	// 6. 设置视野
	tlsRegistryManager.actorRegistry.emplace<ViewRadius>(player).set_radius(10);

	// 7. 设置玩家节点映射（如 Centre NodeId）
	auto& sessionSnapshot = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(player);
	auto& nodeIdMap = *sessionSnapshot.mutable_node_id();
	nodeIdMap[eNodeType::CentreNodeService] = enterInfo.centre_node_id();

	LOG_INFO << "[InitPlayerFromAllData] Set CentreNodeId: " << enterInfo.centre_node_id();

	// 8. 初始化系统组件事件
	InitializeActorComponentsEvent initActorEvent;
	initActorEvent.set_actor_entity(entt::to_integral(player));
	dispatcher.trigger(initActorEvent);

	InitializePlayerComponentsEvent initPlayerEvent;
	initPlayerEvent.set_actor_entity(entt::to_integral(player));
	dispatcher.trigger(initPlayerEvent);

	// 9. 进入场景节点
	EnterGs(player, enterInfo);

	return player;
}

void PlayerNodeSystem::SavePlayerToRedis(entt::entity player)
{
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "[SavePlayerToRedis] Invalid player entity";
		return;
	}

	auto playerId = tlsRegistryManager.actorRegistry.get<Guid>(player);

	using SaveMessage = PlayerDataRedis::element_type::MessageValuePtr;
	SaveMessage message = std::make_shared<SaveMessage::element_type>();

	PlayerAllDataMessageFieldsUnMarshal(player, *message);

	tlsRedisSystem.GetPlayerDataRedis()->Save(message, playerId);

	LOG_INFO << "[SavePlayerToRedis] Player " << playerId << " saved to Redis";
}
