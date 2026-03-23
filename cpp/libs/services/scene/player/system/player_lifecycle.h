#pragma once
#include <any>
#include "engine/core/type_define/type_define.h"
#include "proto/common/database/player_cache.pb.h"

class PlayerGameNodeEntryInfoPBComponent;
class ChangeSceneInfoPBComponent;
class PlayerMigrationPbEvent;

class PlayerLifecycleSystem
{
public:
	// What if the player disconnects during async load? Could cause data overwrite.
	static void HandlePlayerAsyncLoaded(Guid player_id, const PlayerAllData& message, const std::any& extra);
	static void HandlePlayerAsyncSaved(Guid player_id, PlayerAllData& message);
	static void EnterScene(const entt::entity player, const PlayerGameNodeEntryInfoPBComponent& enter_info);
	static void NotifyEnterSceneSucceed(entt::entity player, NodeId centre_node_id);
	static void LeaveGs(entt::entity player);
	static void OnPlayerLogin(entt::entity player, uint32_t enter_gs_type);
	static void HandleBindPlayerToGateOK(entt::entity player);
	static void RemovePlayerSession(Guid player_id);
	static void RemovePlayerSession(entt::entity player);
	static void RemovePlayerSessionSilently(Guid playerId);
	static void DestroyPlayer(Guid player_id);
	static void HandleExitGameNode(entt::entity player);
	static void HandleCrossZoneTransfer(entt::entity playerEntity);
	static void HandlePlayerMigration(const PlayerMigrationPbEvent& msg);
	static entt::entity InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEntryInfoPBComponent& enterInfo);
	static void SavePlayerToRedis(entt::entity player);

};

