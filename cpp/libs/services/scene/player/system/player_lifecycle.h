#pragma once
#include <unordered_map>
#include "engine/core/type_define/type_define.h"
#include "engine/infra/storage/redis_client/redis_client.h"
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
	static void HandlePlayerAsyncLoadFailed(Guid player_id,
											MessageAsyncClient<Guid, PlayerAllData>::LoadFailureReason reason);
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

	// Cross-zone migration ACK handler.
	//
	// Called when this node receives a `player_migrate_ack` Kafka message
	// confirming the destination zone successfully loaded the migrating
	// player. Removes PlayerFrozenComp and finally calls DestroyPlayer to
	// release the source-side entity.
	//
	// If the matching Frozen entity is not present (player already destroyed
	// by reaper / manual intervention / restart recovery), this is a no-op
	// — duplicate ACKs are idempotent.
	//
	// See docs/design/cross-zone-readiness-audit.md §3.2 件 3 for the full
	// ACK protocol and §7 for failure handling. The Kafka topic plumbing
	// is task #25 (still pending — this stub only handles the "ACK arrived"
	// half of the protocol).
	static void HandlePlayerMigrationAck(Guid player_id, uint32_t to_zone_id);

	// True iff this player is currently frozen for cross-zone migration.
	// Business systems (AOI / combat / currency / bag / chat / movement)
	// MUST check this and skip writes when it returns true — Frozen entities
	// are conceptually "not on this node anymore" even though the entt entity
	// still exists. See cross-zone-readiness-audit.md §3.2 件 2.
	static bool IsCrossZoneFrozen(entt::entity player);

	static entt::entity InitPlayerFromAllData(const PlayerAllData& playerAllData, const PlayerGameNodeEntryInfoComp& enterInfo);
	static void SavePlayerToRedis(entt::entity player);

	// True iff this player has an in-flight HandleExitGameNode → SavePlayerToRedis
	// cycle that has not yet seen its HandlePlayerAsyncSaved completion. Detected
	// by the presence of the UnregisterPlayer ECS marker on the player's entity.
	//
	// See todo.md #280 / NOTES Part 2 P0. Same-node reconnect is already handled
	// by EnterScene clearing the marker; cross-node reconnect is gated by the
	// player_locator 30s lease. This method exposes the saving state for callers
	// that want to log / reject / wait when the save outruns those mechanisms.
	//
	// THREADING (Review O3, 2026-05-17): MUST be called from the ECS thread
	// only. Internally does tlsEcs.GetPlayer(playerId) +
	// tlsEcs.actorRegistry.any_of<UnregisterPlayer>(...), both of which touch
	// entt's registry. entt is NOT thread-safe; calling this from a Kafka
	// consumer thread, a gRPC handler thread, or a GM tool background thread
	// is a data race against any ECS mutation happening on the loop thread.
	// If you need cross-thread access, post a task to the EventLoop and read
	// the result back via a future / channel.
	static bool IsSaveInFlight(Guid playerId);

};

