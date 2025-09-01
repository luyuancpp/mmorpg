#pragma once
#include <any>
#include "base/core/type_define/type_define.h"
#include "proto/db/mysql_database_table.pb.h"

class PlayerGameNodeEnteryInfoPBComponent;
class ChangeSceneInfoPBComponent;
class PlayerMigrationPbEvent;
class PlayerGameNodeEnteryInfoPBComponent;

class PlayerNodeSystem
{
public:
	//如果异步加载过程中玩家断开链接了？会不会造成数据覆盖
	static void HandlePlayerAsyncLoaded(Guid player_id, const PlayerAllData& message, const std::any& extra);
	static void HandlePlayerAsyncSaved(Guid player_id, PlayerAllData& message);
	static void EnterGs(const entt::entity player, const PlayerGameNodeEnteryInfoPBComponent& enter_info);
	static void NotifyEnterGsSucceed(entt::entity player, NodeId centre_node_id);
	static void LeaveGs(entt::entity player);
	static void OnPlayerLogin(entt::entity player, uint32_t enter_gs_type);
	static void HandleSceneNodePlayerRegisteredAtGateNode(entt::entity player);
	static void RemovePlayerSession(Guid player_id);
	static void RemovePlayerSession(entt::entity player);
	static void RemovePlayerSessionSilently(Guid player_id);
	static void DestroyPlayer(Guid player_id);
	static void HandleExitGameNode(entt::entity player);
	static void HandleCrossZoneTransfer(entt::entity playerEntity);
	static void HandlePlayerMigration(const PlayerMigrationPbEvent& msg);
	static entt::entity InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEnteryInfoPBComponent& enterInfo);
	static void SavePlayerToRedis(entt::entity player);

};

