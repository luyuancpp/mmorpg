#pragma once
#include <unordered_map>
#include "engine/core/type_define/type_define.h"
#include "proto/common/database/player_cache.pb.h"
#include "proto/common/component/player_async_comp.pb.h"

class ChangeSceneInfoComp;
class PlayerMigrationEvent;

// Map of player_id → enter-info for players whose Redis load is in flight.
// Replaces the std::any extra_data on MessageAsyncClient.
using PendingEnterMap = std::unordered_map<Guid, PlayerGameNodeEntryInfoComp>;

class PlayerLifecycleSystem
{
public:
	static PendingEnterMap& GetPendingEnterMap();

	static void HandlePlayerAsyncLoaded(Guid player_id, const PlayerAllData& message);
	static void HandlePlayerAsyncSaved(Guid player_id, PlayerAllData& message);
	static void EnterScene(const entt::entity player, const PlayerGameNodeEntryInfoComp& enter_info);
	static void HandleBindPlayerToGateOK(entt::entity player);
	static void RemovePlayerSession(Guid player_id);
	static void RemovePlayerSession(entt::entity player);
	static void RemovePlayerSessionSilently(Guid playerId);
	static void DestroyPlayer(Guid player_id);
	static void HandleExitGameNode(entt::entity player);
	static void HandleCrossZoneTransfer(entt::entity playerEntity);
	static void HandlePlayerMigration(const PlayerMigrationEvent& msg);
	static entt::entity InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEntryInfoComp& enterInfo);
	static void SavePlayerToRedis(entt::entity player);

};

