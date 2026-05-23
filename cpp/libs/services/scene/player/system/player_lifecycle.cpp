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
#include "core/utils/proto/proto_dirty_compare.h"
#include "network/node_utils.h"
#include "player/comp/last_persisted_snapshot_comp.h"
#include "player/comp/player_frozen_comp.h"
#include "player/system/cross_zone_reaper.h"
#include "player/system/player_data_loader.h"
#include "engine/core/type_define/type_define.h"
#include "proto/common/event/player_migration_event.pb.h"
#include "table/proto/tip/cross_server_error_tip.pb.h"
#include "player_tip.h"
#include "modules/scene/comp/scene_comp.h"
#include "modules/scene/comp/scene_node_comp.h"
#include <engine/infra/messaging/kafka/kafka_producer.h>
#include "core/system/redis.h"
#include "player_scene.h"
#include "stress_test_probe.h"
#include "player/constants/player.h"
#include "proto/db/db_task.pb.h"
#include "modules/snapshot/snapshot_system.h"
#include <proto/scene/scene_info.pb.h>
#include "player/comp/afk_comp.h"
#include "frame/manager/frame_time.h"
#include "proto/common/event/scene_event.pb.h"
#include "network/network_utils.h"
#include "network/player_message_utils.h"
#include "network/rpc_session.h"
#include "thread_context/node_context_manager.h"
#include "rpc/service_metadata/client_player_common_service_metadata.h"
#include "table/proto/tip/scene_error_tip.pb.h"

thread_local PendingEnterMap tlsPendingEnterMap;

PendingEnterMap& PlayerLifecycleSystem::GetPendingEnterMap()
{
	return tlsPendingEnterMap;
}

namespace
{
	// Sends a SendTipToClient message directly to the gate by session_id.
	// Used during the async-load window when the player entity does not yet
	// exist, so the entity-based PlayerTipSystem path is not available.
	void SendTipToPendingSession(SessionId sessionId, uint32_t tipId)
	{
		if (sessionId == 0)
		{
			return;
		}
		entt::entity gateEntity{GetGateNodeId(sessionId)};
		auto &gateNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
		if (!gateNodeRegistry.valid(gateEntity))
		{
			LOG_WARN << "SendTipToPendingSession: gate not found for session " << sessionId;
			return;
		}
		auto *gateSessionPtr = gateNodeRegistry.try_get<RpcSession>(gateEntity);
		if (gateSessionPtr == nullptr)
		{
			LOG_WARN << "SendTipToPendingSession: RpcSession missing for session " << sessionId;
			return;
		}
		TipInfoMessage tip;
		tip.set_id(tipId);
		SendMessageToClientViaGate(SceneClientPlayerCommonSendTipToClientMessageId,
								   tip, *gateSessionPtr, sessionId);
	}
} // namespace

void PlayerLifecycleSystem::HandlePlayerAsyncLoadFailed(Guid playerId,
														MessageAsyncClient<Guid, PlayerAllData>::LoadFailureReason reason)
{
	using LoadFailureReason = MessageAsyncClient<Guid, PlayerAllData>::LoadFailureReason;

	// DataNotFound: NIL after exhausting retries. Treat as a brand-new player
	// whose DB rows do not exist yet (CreatePlayer only writes account meta;
	// PlayerAllData parent key is first written by Scene's own SavePlayerToRedis,
	// so a first-time login will always observe NIL here). Hand off to the
	// existing new-player branch in HandlePlayerAsyncLoaded by feeding it an
	// empty PlayerAllData -- it detects player_database_data().player_id()==0
	// and stamps the playerId from the async-load key.
	if (reason == LoadFailureReason::DataNotFound)
	{
		LOG_INFO << "HandlePlayerAsyncLoadFailed: no Redis data for player " << playerId
				 << " after retries, treating as brand-new player";
		PlayerAllData empty;
		HandlePlayerAsyncLoaded(playerId, empty);
		return;
	}

	// RedisError: connection lost, parse failure, or unexpected reply type.
	// We cannot proceed -- notify the client (if a session is still bound) so
	// it leaves the loading screen instead of hanging, then drop pending state.
	LOG_ERROR << "HandlePlayerAsyncLoadFailed: Redis load failed for player " << playerId;

	auto pendingIt = tlsPendingEnterMap.find(playerId);
	if (pendingIt != tlsPendingEnterMap.end())
	{
		const SessionId sessionId = pendingIt->second.session_id();
		if (sessionId != 0)
		{
			// Best-effort tip; safe even if the gate session is already gone.
			SendTipToPendingSession(sessionId, kEnterSceneFailed);
			SessionMap().erase(sessionId);
		}
		tlsPendingEnterMap.erase(pendingIt);
	}
}

void PlayerLifecycleSystem::HandlePlayerAsyncLoaded(Guid playerId, const PlayerAllData &message)
{
	LOG_INFO << "HandlePlayerAsyncLoaded: Loading player " << playerId;

	// Consume the pending enter info. If absent, the load was orphaned.
	auto pendingIt = tlsPendingEnterMap.find(playerId);
	if (pendingIt == tlsPendingEnterMap.end())
	{
		LOG_WARN << "HandlePlayerAsyncLoaded: no pending enter info for player " << playerId << ", skipping";
		return;
	}
	PlayerGameNodeEntryInfoComp enterInfo = std::move(pendingIt->second);
	tlsPendingEnterMap.erase(pendingIt);

	// If the session was erased during async load (e.g. client disconnected
	// and ExitGame arrived before load completed), skip entity creation.
	if (enterInfo.session_id() != 0
		&& SessionMap().find(enterInfo.session_id()) == SessionMap().end())
	{
		LOG_INFO << "HandlePlayerAsyncLoaded: session " << enterInfo.session_id()
		         << " cancelled during async load for player " << playerId << ", skipping";
		return;
	}

	// If the loaded data has player_id=0, this is a brand-new player whose DB
	// rows don't exist yet (CreatePlayer only creates an account entry), or an
	// orphan after zone rollback.  In either case, set the player_id from the
	// async-load key and let InitPlayerFromAllData handle first-time registration
	// via the registration_timestamp check.
	const PlayerAllData *data = &message;
	PlayerAllData patchedMessage;
	if (message.player_database_data().player_id() == 0)
	{
		LOG_INFO << "HandlePlayerAsyncLoaded: No existing DB data for player " << playerId
				 << ", will initialize as new player";
		patchedMessage = message;
		patchedMessage.mutable_player_database_data()->set_player_id(playerId);
		patchedMessage.mutable_player_database_1_data()->set_player_id(playerId);
		data = &patchedMessage;
	}

	InitPlayerFromAllData(*data, enterInfo);
}

void PlayerLifecycleSystem::HandlePlayerAsyncSaved(Guid playerId, PlayerAllData &message)
{
	LOG_INFO << "HandlePlayerAsyncSaved: Saving complete for player: " << playerId;

	// TODO: When should session be deleted?

	auto playerEntity = tlsEcs.GetPlayer(playerId);
	HandleCrossZoneTransfer(playerEntity);

	// Cross-zone in flight — DO NOT destroy here.
	// HandleCrossZoneTransfer set PlayerFrozenComp on the entity above and
	// published `player_migrate` to Kafka. The entity must stay alive
	// (write-disabled via the Frozen check in business systems) until either:
	//   • the destination's `player_migrate_ack` arrives (ACK handler then
	//     removes PlayerFrozenComp and calls DestroyPlayer);
	//   • the reaper declares the migration failed (it then removes
	//     PlayerFrozenComp and unfreezes the entity so the player keeps
	//     playing on the source side).
	//
	// Before this change, the code fell straight through to DestroyPlayer
	// after Kafka send. If the broker dropped the message or the destination
	// node crashed, the player vanished on BOTH sides. See
	// cross-zone-readiness-audit.md §1 失败 B and §3.2 件 2.
	//
	// IMPORTANT: this gate must come BEFORE the UnregisterPlayer branch
	// below — HandleExitGameNode emplaces UnregisterPlayer on the entity
	// before SavePlayerToRedis runs, so a cross-zone path also carries that
	// tag. We want PlayerFrozenComp to win.
	if (tlsEcs.actorRegistry.valid(playerEntity) &&
		tlsEcs.actorRegistry.any_of<PlayerFrozenComp>(playerEntity))
	{
		LOG_INFO << "HandlePlayerAsyncSaved: player " << playerId
				 << " is frozen for cross-zone migration; deferring destroy until ACK or reaper.";
		// Still update last-persisted snapshot below so the dirty-save fast
		// path stays correct if the migration fails and the player resumes.
	}
	else if (tlsEcs.actorRegistry.any_of<UnregisterPlayer>(playerEntity))
	{
		// Detect saves that outran the player_locator reconnect lease (30s).
		// See todo.md #280, layer 3. logout_initiated_ms is stamped in
		// HandleExitGameNode; default value 0 from older proto runs is treated
		// as "unknown" and silently skipped.
		const auto& unregisterTag = tlsEcs.actorRegistry.get<UnregisterPlayer>(playerEntity);
		const int64_t logoutMs = unregisterTag.logout_initiated_ms();
		if (logoutMs > 0)
		{
			constexpr int64_t kReconnectLeaseMs = 30 * 1000;
			const int64_t elapsedMs = TimeSystem::NowMillisecondsUTC() - logoutMs;
			if (elapsedMs > kReconnectLeaseMs)
			{
				LOG_WARN << "HandlePlayerAsyncSaved: save outran reconnect lease for player "
						 << playerId << " — elapsed_ms=" << elapsedMs
						 << " lease_ms=" << kReconnectLeaseMs
						 << " (cross-node re-login during this window may have read stale data)";
			}
		}

		// Defense in depth: if a reconnect rebound this entity to a live session
		// after the save was enqueued, the UnregisterPlayer tag should have been
		// cleared by EnterScene. If a tag still slipped through but the player
		// has an active session, do NOT destroy — drop the stale unregister intent.
		const auto *snapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(playerEntity);
		if (snapshot != nullptr && snapshot->gate_session_id() != kInvalidSessionId)
		{
			const auto sessionIt = SessionMap().find(snapshot->gate_session_id());
			if (sessionIt != SessionMap().end() && sessionIt->second == playerId)
			{
				LOG_WARN << "HandlePlayerAsyncSaved: ignoring stale UnregisterPlayer for player "
						 << playerId << " — live session " << snapshot->gate_session_id()
						 << " indicates reconnect superseded the logout intent";
				tlsEcs.actorRegistry.remove<UnregisterPlayer>(playerEntity);
				return;
			}
		}

		LOG_INFO << "Player marked for unregistration: " << playerId;

		// Must complete save before deleting session; consider better ordering
		RemovePlayerSession(playerId);
		LOG_INFO << "Player session removed";
		// TODO: Verify no race condition on destroy-after-save ordering
		DestroyPlayer(playerId);
	}

	// Update last-persisted snapshot (todo.md #204 / #226 slice B).
	// Successful save means the bytes in `message` are now what's in
	// Redis; record them so the next SavePlayerToRedis can do a
	// dirty-equality fast-path check. Skip if the entity was already
	// destroyed above (UnregisterPlayer + DestroyPlayer path) — there's
	// nothing to attach the component to.
	if (tlsEcs.actorRegistry.valid(playerEntity))
	{
		auto& snap = tlsEcs.actorRegistry.get_or_emplace<PlayerLastPersistedSnapshotComp>(playerEntity);
		snap.Replace(message);
	}

	// player_locator lease handles reconnect gating via Redis TTL.
}

// CONSIDER: handle reentry into a different scene node while load is still in progress
void PlayerLifecycleSystem::EnterScene(const entt::entity player, const PlayerGameNodeEntryInfoComp &enterInfo)
{
	const auto playerId = tlsEcs.actorRegistry.get<Guid>(player);
	LOG_DEBUG << "EnterScene: Player " << playerId << " entering scene node"
	         << " session=" << enterInfo.session_id()
	         << " scene_id=" << enterInfo.scene_id()
	         << " enter_gs_type=" << enterInfo.enter_gs_type();

	// 0. Cancel any pending unregistration. If this player previously called
	//    HandleExitGameNode (disconnect) but the async save hasn't completed yet,
	//    the entity still carries the UnregisterPlayer tag and the save callback
	//    will destroy it. Reconnect supersedes the logout intent — clear the tag
	//    so HandlePlayerAsyncSaved leaves the live entity alone.
	if (tlsEcs.actorRegistry.any_of<UnregisterPlayer>(player))
	{
		tlsEcs.actorRegistry.remove<UnregisterPlayer>(player);
		LOG_INFO << "EnterScene: cancelled pending unregistration for reconnected player " << playerId;
	}

	// 1. Bind session: map session_id -> player_id on this Scene node
	//    so SendMessageToPlayer can route by session, and
	//    SendMessageToClientViaGate can find the gate node.
	if (enterInfo.session_id() != 0)
	{
		// Clean up old session mapping if player already had a different session
		// (e.g. reconnect with new Gate connection). Prevents orphaned entries.
		auto &snapshot = tlsEcs.actorRegistry.get_or_emplace<PlayerSessionSnapshotComp>(player);
		const auto oldSessionId = snapshot.gate_session_id();
		if (oldSessionId != 0 && oldSessionId != enterInfo.session_id())
		{
			SessionMap().erase(oldSessionId);
			LOG_INFO << "EnterScene: cleaned up old session " << oldSessionId
			         << " for player " << playerId;
		}

		SessionMap().insert_or_assign(enterInfo.session_id(), playerId);
		snapshot.set_gate_session_id(enterInfo.session_id());
		snapshot.set_player_id(playerId);
	}

	// 2. Find the target scene entity by scene_id (allocated by SceneManager).
	entt::entity targetScene = entt::null;
	if (enterInfo.scene_id() != 0)
	{
		auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
		for (auto entity : view)
		{
			const auto &info = view.get<SceneInfoComp>(entity);
			if (info.scene_id() == enterInfo.scene_id())
			{
				targetScene = entity;
				break;
			}
		}

		if (targetScene == entt::null)
		{
			LOG_ERROR << "EnterScene: scene_id=" << enterInfo.scene_id()
			          << " not found on this node for player " << playerId;
		}
	}

	// 3. Enter the scene: bind player to scene entity and send client notification.
	if (targetScene != entt::null)
	{
		PlayerSceneSystem::HandleEnterScene(player, targetScene);
	}

	// 4. Set login state for downstream systems (reconnect, first-login logic, etc.).
	if (enterInfo.enter_gs_type() != 0)
	{
		auto &enterState = tlsEcs.actorRegistry.get_or_emplace<PlayerEnterGameStateComp>(player);
		const bool alreadyLoggedIn = (enterState.enter_gs_type() != 0);
		enterState.set_enter_gs_type(enterInfo.enter_gs_type());

		// 5. Fire login event only on first entry — skip on duplicate/reconnect
		//    to avoid business systems (quests, daily rewards, etc.) reacting twice.
		if (!alreadyLoggedIn)
		{
			PlayerLoginEvent loginEvent;
			loginEvent.set_actor_entity(entt::to_integral(player));
			loginEvent.set_enter_gs_type(enterInfo.enter_gs_type());
			tlsEcs.dispatcher.trigger(loginEvent);
		}
	}
}



void PlayerLifecycleSystem::HandleBindPlayerToGateOK(entt::entity player)
{
	// TODO: notify client that gate binding is ready; send initial scene state snapshot
}

// TODO: Validate session before removal
void PlayerLifecycleSystem::RemovePlayerSession(const Guid playerId)
{
	auto playerIt = tlsEcs.playerList.find(playerId);
	if (playerIt == tlsEcs.playerList.end())
	{
		LOG_ERROR << "RemovePlayerSession: player entity not found in session map for player: " << playerId;
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerLifecycleSystem::RemovePlayerSession(entt::entity player)
{
	auto *const playerSessionSnapshotPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
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
	auto playerIt = tlsEcs.playerList.find(playerId);
	if (playerIt == tlsEcs.playerList.end())
	{
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerLifecycleSystem::DestroyPlayer(Guid playerId)
{
	LOG_INFO << "Destroying player: " << playerId;

	defer(tlsEcs.playerList.erase(playerId));
	DestroyEntity(tlsEcs.actorRegistry, tlsEcs.GetPlayer(playerId));
}

void PlayerLifecycleSystem::HandleExitGameNode(entt::entity player)
{
	const auto* g = tlsEcs.actorRegistry.try_get<Guid>(player);
	LOG_INFO << "HandleExitGameNode: Player " << (g ? *g : 0) << " is exiting the scene node";

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "HandleExitGameNode: Player entity is not valid";
		return;
	}

	if (tlsEcs.actorRegistry.all_of<UnregisterPlayer>(player))
	{
		LOG_INFO << "Player " << (g ? *g : 0) << " is already marked for unregistration";
		return;
	}

	auto& unregisterTag = tlsEcs.actorRegistry.emplace<UnregisterPlayer>(player);
	unregisterTag.set_logout_initiated_ms(TimeSystem::NowMillisecondsUTC());

	// Remove entity from AOI grid immediately so the AOI system stops
	// sending messages to the (already-disconnected) gate session.
	if (tlsEcs.actorRegistry.any_of<SceneEntityComp>(player))
	{
		BeforeLeaveScene leaveEvent;
		leaveEvent.set_entity(entt::to_integral(player));
		tlsEcs.dispatcher.trigger(leaveEvent);
		tlsEcs.actorRegistry.remove<SceneEntityComp>(player);
	}

	// Capture a logout snapshot before persisting (safety net for rollback).
	SnapshotSystem::CaptureAndSend(player, SNAPSHOT_LOGOUT);

	PlayerLifecycleSystem::SavePlayerToRedis(player);

	// Re-login race protection (todo.md #280). The save is now in flight.
	// THREE layers cover the read-stale-data window between SavePlayerToRedis
	// being enqueued here and HandlePlayerAsyncSaved firing:
	//
	//   1. Same-node reconnect — EnterScene() clears the UnregisterPlayer tag
	//      so HandlePlayerAsyncSaved drops the stale unregister intent and
	//      leaves the live entity alone (see EnterScene step 0).
	//
	//   2. Cross-node reconnect — player_locator holds a 30s Redis lease
	//      (DefaultTTLSeconds in player_locator.yaml). Any re-login within
	//      that window is steered back to the original node, which falls
	//      back to layer 1.
	//
	//   3. Save-exceeds-lease anomaly — if Redis/Kafka backoff pushes the
	//      save past 30s the player CAN appear on a fresh node before
	//      HandlePlayerAsyncSaved fires. HandlePlayerAsyncSaved logs a
	//      "save outran reconnect lease" warning by comparing
	//      logout_initiated_ms; ops watch the warning and follow up if
	//      it's not just a one-off spike.
	//
	// IsSaveInFlight() exposes layers 1–2 for callers that want to query
	// rather than rely on the implicit ECS-marker convention.
}

void PlayerLifecycleSystem::HandleCrossZoneTransfer(entt::entity playerEntity)
{
	auto changeInfo = tlsEcs.actorRegistry.try_get<ChangeSceneInfoComp>(playerEntity);
	if (!changeInfo)
	{
		return;
	}

	if (!changeInfo->is_cross_zone())
	{
		return;
	}

	auto playerId = tlsEcs.actorRegistry.get<Guid>(playerEntity);

	PlayerAllData playerAllDataMessage;
	PlayerAllDataMessageFieldsMarshal(playerEntity, playerAllDataMessage);
	playerAllDataMessage.mutable_player_database_data()->set_player_id(playerId);
	playerAllDataMessage.mutable_player_database_1_data()->set_player_id(playerId);

	// ⚠️ KNOWN GAP (cross-zone-readiness-audit.md §1.3): PlayerAllData currently
	// only carries the 7 ECS components present in player_database. bag, quest,
	// and mail data are NOT in the proto and will silently vanish on every
	// cross-zone transition until the BagAllData / QuestAllData / MailAllData
	// sub-messages are added (audit doc §3.2 件 1, task #23).

	const auto toZoneId = changeInfo->to_zone_id();

	PlayerMigrationEvent request;
	request.set_player_id(playerId);
	request.set_from_zone(GetZoneId());
	request.set_to_zone(toZoneId);
	request.mutable_scene_info()->CopyFrom(*changeInfo);
	request.set_serialized_player_data(std::move(playerAllDataMessage.SerializeAsString()));

	KafkaProducer::Instance().send("player_migrate", request.SerializeAsString(), std::to_string(playerId), toZoneId);

	LOG_INFO << "[CrossZone] Sent player transfer to zone " << toZoneId << ": " << playerId;

	PlayerTipSystem::SendToPlayer(playerEntity, kSceneTransferInProgress, {});

	// Freeze the source-side entity instead of immediately destroying it.
	// Before this change, `DestroyPlayer` ran in HandlePlayerAsyncSaved right
	// after the Kafka send — meaning a Kafka broker failure or destination-node
	// crash would lose the player on BOTH sides (source destroyed, destination
	// never received). See cross-zone-readiness-audit.md §1 失败 B.
	//
	// PlayerFrozenComp keeps the entity alive but write-disabled until either:
	//   • The destination publishes a `player_migrate_ack` (success path) —
	//     the ACK handler removes the component and calls DestroyPlayer.
	//   • The reaper declares the migration failed after retries (failure
	//     path) — the reaper removes the component and unfreezes the entity
	//     so the player keeps playing on the source side.
	//
	// All business systems (AOI / combat / currency / bag / etc.) must
	// gate on `actorRegistry.any_of<PlayerFrozenComp>(player)` to skip
	// writes. The audit doc §3.2 件 2 enumerates the systems that need
	// the gate (still being threaded through — task #26).
	auto& frozen = tlsEcs.actorRegistry.emplace_or_replace<PlayerFrozenComp>(playerEntity);
	frozen.frozenAtMs = TimeSystem::NowMillisecondsUTC();
	frozen.toZoneId = toZoneId;
	frozen.migrateAttempts = 1;

	// Persist a Redis migration record so the reaper can recover this
	// migration if the Kafka publish above is lost / destination crashes /
	// THIS scene node restarts before ACK arrives. attempt=1 because this
	// is the original publish; the reaper bumps attempt on republish.
	// See docs/design/cross-zone-readiness-audit.md §3.2 件 3.
	CrossZoneReaper::RecordMigrationStart(
		playerId, GetZoneId(), toZoneId, /*toNodeId=*/0u, /*attempt=*/1u);

	tlsEcs.actorRegistry.remove<ChangeSceneInfoComp>(playerEntity);
}

void PlayerLifecycleSystem::HandlePlayerMigration(const PlayerMigrationEvent &msg)
{
	// Idempotency guard — see cross-zone-failure-test-runbook.md §失败 D and
	// task #32. Kafka rebalance can redeliver player_migrate after we already
	// processed it (or the source's reaper republishes during a slow ACK
	// window). If the player entity is already present on this node, the
	// previous Init must have succeeded — we only need to re-emit the ACK so
	// the source's reaper / DestroyPlayer path completes. Re-running Init
	// would create a second entt entity (with the same player_id but a
	// different entity handle) and the player would "double-spawn" — items
	// flooded into bag, duplicate AOI entries, etc.
	const auto existingPlayer = tlsEcs.GetPlayer(msg.player_id());
	if (existingPlayer != entt::null && tlsEcs.actorRegistry.valid(existingPlayer))
	{
		LOG_INFO << "[CrossZone] HandlePlayerMigration: player " << msg.player_id()
				 << " from zone " << msg.from_zone() << " already present on this node "
				 << "(duplicate delivery / source reaper retry); skipping Init, "
				 << "re-emitting ACK so source can converge.";
		// Fall through to the ACK publish below. SavePlayerToRedis would also
		// be redundant here (player is already loaded), so skip it too.
		PlayerMigrationAckEvent ackEvent;
		ackEvent.set_player_id(msg.player_id());
		ackEvent.set_from_zone(msg.from_zone());
		ackEvent.set_to_zone(msg.to_zone());
		ackEvent.set_ack_at_ms(TimeSystem::NowMillisecondsUTC());

		std::string ackBytes;
		if (ackEvent.SerializeToString(&ackBytes))
		{
			KafkaProducer::Instance().send(
				"player_migrate_ack", ackBytes, std::to_string(msg.player_id()), msg.from_zone());
		}
		return;
	}

	PlayerAllData playerAllDataMessage;
	if (!playerAllDataMessage.ParseFromString(msg.serialized_player_data()))
	{
		LOG_ERROR << "Parse failed for player migration data";
		return;
	}

	PlayerGameNodeEntryInfoComp enterInfo;

	auto player = InitPlayerFromAllData(playerAllDataMessage, enterInfo);
	if (!tlsEcs.actorRegistry.valid(player))
	{
		// InitPlayerFromAllData rejected the payload (player_id=0 or other
		// validation failure). Do NOT publish an ACK — the source needs to
		// see the migration as failed and let the reaper retry / declare
		// it lost.
		LOG_ERROR << "[CrossZone] HandlePlayerMigration rejected player_id="
				  << msg.player_id() << " from zone " << msg.from_zone()
				  << "; no ACK will be sent.";
		return;
	}

	SavePlayerToRedis(player);

	// Publish `player_migrate_ack` so the source-side scene node can clear
	// its PlayerFrozenComp and DestroyPlayer. Without this ACK the source
	// entity sits frozen forever (or until the reaper times it out and
	// unfreezes it, leading to double-presence).
	//
	// Payload is a `PlayerMigrationAckEvent` proto (see
	// proto/common/event/player_migration_event.proto). We use protobuf
	// rather than JSON for consistency with every other Kafka payload in
	// this codebase — JSON parsing is an order of magnitude slower and
	// costs us schema versioning / type safety.
	//
	// Kafka partition_key is the same playerId used by `player_migrate`, so
	// ACK arrives on the same partition's downstream — preserving ordering
	// with any subsequent migrations of the same player.
	PlayerMigrationAckEvent ackEvent;
	ackEvent.set_player_id(msg.player_id());
	ackEvent.set_from_zone(msg.from_zone());
	ackEvent.set_to_zone(msg.to_zone());
	ackEvent.set_ack_at_ms(TimeSystem::NowMillisecondsUTC());

	std::string ackBytes;
	if (!ackEvent.SerializeToString(&ackBytes))
	{
		LOG_ERROR << "[CrossZone] Failed to serialize PlayerMigrationAckEvent for player "
				  << msg.player_id() << " — source-side reaper will eventually retry the migration.";
		return;
	}

	auto ackErr = KafkaProducer::Instance().send(
		"player_migrate_ack", ackBytes, std::to_string(msg.player_id()), msg.from_zone());
	if (ackErr != RdKafka::ERR_NO_ERROR)
	{
		LOG_ERROR << "[CrossZone] Failed to publish ACK for player " << msg.player_id()
				  << " from_zone=" << msg.from_zone() << " err=" << RdKafka::err2str(ackErr)
				  << " — source-side reaper will eventually retry the migration.";
	}
	else
	{
		LOG_INFO << "[CrossZone] Published ACK for player " << msg.player_id()
				 << " (from_zone=" << msg.from_zone() << ", to_zone=" << msg.to_zone() << ").";
	}
}

entt::entity PlayerLifecycleSystem::InitPlayerFromAllData(const PlayerAllData &playerAllData, const PlayerGameNodeEntryInfoComp &enterInfo)
{
	auto playerId = playerAllData.player_database_data().player_id();

	if (playerId == 0)
	{
		LOG_ERROR << "[InitPlayerFromAllData] Rejecting player with id=0 (empty data)";
		return entt::null;
	}

	LOG_INFO << "[InitPlayerFromAllData] Init player: " << playerId;

	auto player = tlsEcs.actorRegistry.create();

	// Register in global player-entity map
	if (const auto [it, inserted] = tlsEcs.playerList.emplace(playerId, player); !inserted)
	{
		LOG_ERROR << "[InitPlayerFromAllData] Player already exists in GlobalPlayerList: " << playerId;
		return entt::null;
	}

	tlsEcs.actorRegistry.emplace<Player>(player);
	tlsEcs.actorRegistry.emplace<Guid>(player, playerId);
	tlsEcs.actorRegistry.emplace<LastActiveFrameComp>(player, tlsFrameTimeManager.frameTime.current_frame());

	PlayerAllDataMessageFieldsUnMarshal(player, playerAllData);

	// First-time registration: initialize defaults
	if (playerAllData.player_database_data().uint64_pb_component().registration_timestamp() <= 0)
	{
		tlsEcs.actorRegistry.get_or_emplace<PlayerUint64Comp>(player).set_registration_timestamp(TimeSystem::NowSecondsUTC());
		tlsEcs.actorRegistry.get_or_emplace<LevelComp>(player).set_level(1);

		RegisterPlayerEvent registerPlayer;
		registerPlayer.set_actor_entity(entt::to_integral(player));
		tlsEcs.dispatcher.trigger(registerPlayer);
	}

	tlsEcs.actorRegistry.emplace<ViewRadius>(player).set_radius(10);

	// player_locator (Go) owns the canonical session/location record.

	// Fire component initialization events
	InitializeActorCompsEvent initActorEvent;
	initActorEvent.set_actor_entity(entt::to_integral(player));
	tlsEcs.dispatcher.trigger(initActorEvent);

	InitializePlayerCompsEvent initPlayerEvent;
	initPlayerEvent.set_actor_entity(entt::to_integral(player));
	tlsEcs.dispatcher.trigger(initPlayerEvent);

	EnterScene(player, enterInfo);

	// Capture a login snapshot for rollback safety net.
	SnapshotSystem::CaptureAndSend(player, SNAPSHOT_LOGIN);

	return player;
}

void PlayerLifecycleSystem::SavePlayerToRedis(entt::entity player)
{
	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "[SavePlayerToRedis] Invalid player entity";
		return;
	}

	auto playerId = tlsEcs.actorRegistry.get<Guid>(player);

	using SaveMessage = PlayerDataRedis::element_type::MessageValuePtr;
	SaveMessage message = std::make_shared<SaveMessage::element_type>();

	PlayerAllDataMessageFieldsMarshal(player, *message);

	// Set player_id on each sub-table (not set by generated marshal code).
	// MUST be done BEFORE Save(): MessageAsyncClient::Save() now serializes the
	// payload eagerly into Element::serialized_payload, so any post-Save mutation
	// would not make it into the Redis blob.
	message->mutable_player_database_data()->set_player_id(playerId);
	message->mutable_player_database_1_data()->set_player_id(playerId);

	// Dirty-save fast path (todo.md #204 / #226 slice B).
	// MUST run BEFORE stresstest_probe::Stamp* below (Review R2 fix,
	// 2026-05-17). The probe writes non-business fields (timestamps /
	// counters) into the message which would otherwise poison the
	// equality check — under STRESS_TEST_PROBE the stamped fields
	// change every call, so a post-stamp IsEqual always returns false
	// and the optimization is silently disabled. Compare clean
	// business data here, stamp probe only on the path that actually
	// persists.
	//
	// Skipping is safe because:
	//   - The previous save already committed identical bytes; reading
	//     them back yields the same state.
	//   - HandlePlayerAsyncSaved updates the snapshot ONLY after a
	//     successful save, so a snapshot-equality match means the
	//     last persisted state matches what we'd write now.
	//   - First save (no snapshot present) always falls through and
	//     writes — `ShouldPersist(current, nullptr)` returns true.
	//
	// Limitation: snapshot lives only on the live entity. A reconnect
	// that destroys + re-creates the entity loses the snapshot, so the
	// first save after EnterScene always writes. That's intentional —
	// we'd rather pay one redundant write than risk skipping a save
	// when the in-memory state diverged from Redis during a load path
	// we don't fully trust.
	if (auto* snap = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
		snap != nullptr && snap->HasSnapshot() &&
		dirty_save::IsEqual(*message, *snap->snapshot))
	{
		LOG_DEBUG << "[SavePlayerToRedis] no-op for player " << playerId
				  << " — proto-compare clean, last_save_ms=" << snap->saved_at_ms;
		return;
	}

	// Stamp the data-consistency stress probe (no-op when STRESS_TEST_PROBE
	// is unset). MUST be before Save() — payload is serialized eagerly
	// inside Save(). Stamped after the dirty-save check (see R2 note above)
	// so probe fields don't pollute the equality comparison.
	stresstest_probe::StampPlayerDatabase(*message->mutable_player_database_data());
	stresstest_probe::StampPlayerDatabase1(*message->mutable_player_database_1_data());

	tlsRedisSystem.GetPlayerDataRedis()->Save(message, playerId);

	// Send each sub-table as a separate DBTask (matching how login reads per-table)
	const std::string playerIdStr = std::to_string(playerId);

	auto sendSubTableTask = [&](const google::protobuf::Message &subMsg)
	{
		const std::string tableName(subMsg.GetDescriptor()->full_name());

		std::string bodyBytes;
		if (!subMsg.SerializeToString(&bodyBytes))
		{
			LOG_ERROR << "[SavePlayerToRedis] Serialize failed: table=" << tableName
					  << " player=" << playerId;
			return;
		}

		taskpb::DBTask dbTask;
		dbTask.set_key(playerId);
		dbTask.set_op("write");
		dbTask.set_msg_type(tableName);
		dbTask.set_body(std::move(bodyBytes));
		dbTask.set_task_id(playerIdStr + ":" + tableName + ":" + std::to_string(TimeSystem::NowMillisecondsUTC()));

		std::string dbTaskBytes;
		if (!dbTask.SerializeToString(&dbTaskBytes))
		{
			LOG_ERROR << "[SavePlayerToRedis] DBTask serialize failed: table=" << tableName
					  << " player=" << playerId;
			return;
		}

		auto err = KafkaProducer::Instance().send(GetDbTaskTopic(GetZoneId()), dbTaskBytes, playerIdStr);
		if (err != RdKafka::ERR_NO_ERROR)
		{
			LOG_ERROR << "[SavePlayerToRedis] Kafka send failed: table=" << tableName
					  << " player=" << playerId << " err=" << RdKafka::err2str(err);
		}
	};

	sendSubTableTask(message->player_database_data());
	sendSubTableTask(message->player_database_1_data());

	LOG_INFO << "[SavePlayerToRedis] Player " << playerId << " saved to Redis, DB write tasks enqueued";
}

bool PlayerLifecycleSystem::IsSaveInFlight(Guid playerId)
{
	// "In flight" == HandleExitGameNode has stamped the UnregisterPlayer marker
	// AND HandlePlayerAsyncSaved has not yet fired (which would either drop the
	// marker on a reconnect-superseded entity, or destroy the entity outright).
	//
	// Once the entity is destroyed, tlsEcs.GetPlayer(playerId) returns
	// entt::null and any_of<UnregisterPlayer> on null returns false — so the
	// "save complete" terminal state is naturally represented by "no entity".
	const auto playerEntity = tlsEcs.GetPlayer(playerId);
	if (!tlsEcs.actorRegistry.valid(playerEntity))
	{
		return false;
	}
	return tlsEcs.actorRegistry.any_of<UnregisterPlayer>(playerEntity);
}

// ─────────────────────────────────────────────────────────────────────
// Cross-zone migration ACK / Frozen state helpers
//
// See docs/design/cross-zone-readiness-audit.md §3.2 件 2-3 for the
// design rationale. Frozen state replaces the old "Kafka send → immediate
// DestroyPlayer" pattern that silently lost players on broker / dest
// failure.
// ─────────────────────────────────────────────────────────────────────

bool PlayerLifecycleSystem::IsCrossZoneFrozen(entt::entity player)
{
	if (!tlsEcs.actorRegistry.valid(player))
	{
		return false;
	}
	return tlsEcs.actorRegistry.any_of<PlayerFrozenComp>(player);
}

void PlayerLifecycleSystem::HandlePlayerMigrationAck(Guid playerId, uint32_t toZoneId)
{
	const auto playerEntity = tlsEcs.GetPlayer(playerId);
	if (!tlsEcs.actorRegistry.valid(playerEntity))
	{
		// Already destroyed (reaper / restart recovery beat us, or this is a
		// duplicate ACK from Kafka rebalance). Idempotent no-op.
		LOG_INFO << "[CrossZone] ACK for player " << playerId
				 << " (zone=" << toZoneId
				 << ") arrived but entity is gone — assuming already-handled, ignoring.";
		return;
	}

	const auto* frozen = tlsEcs.actorRegistry.try_get<PlayerFrozenComp>(playerEntity);
	if (frozen == nullptr)
	{
		// Entity exists but isn't frozen — something is off. Could be:
		//   • Player reconnected after a failed migration and is live again,
		//     and a stale ACK from the failed attempt arrived late.
		//   • Bug in the migration state machine.
		// Log and bail — refusing to destroy a live player on an unverified ACK.
		LOG_WARN << "[CrossZone] ACK for player " << playerId
				 << " (zone=" << toZoneId
				 << ") arrived but entity is NOT frozen — ignoring (possible stale ACK).";
		return;
	}

	if (frozen->toZoneId != 0 && frozen->toZoneId != toZoneId)
	{
		// ACK from the wrong zone — almost certainly a duplicate from a
		// previous failed migration attempt that the reaper already gave up on.
		// Don't destroy.
		LOG_WARN << "[CrossZone] ACK zone mismatch for player " << playerId
				 << " — frozen.toZoneId=" << frozen->toZoneId
				 << " ack.toZoneId=" << toZoneId << " — ignoring.";
		return;
	}

	LOG_INFO << "[CrossZone] ACK confirmed for player " << playerId
			 << " (zone=" << toZoneId << "); destroying source-side entity.";

	// Order matters:
	//   1. Remove Frozen so any business system that checks IsCrossZoneFrozen
	//      between here and DestroyPlayer doesn't see a stale freeze.
	//   2. Remove gate session — the destination already accepted the player,
	//      the client should have switched binding via gate routing.
	//   3. Destroy entity.
	//   4. Tell the reaper this migration is done so it stops watching the
	//      Redis player_migration:{playerId} key. (DEL is idempotent — if the
	//      reaper TTL beat us to it, this is a harmless no-op.)
	tlsEcs.actorRegistry.remove<PlayerFrozenComp>(playerEntity);
	RemovePlayerSession(playerId);
	DestroyPlayer(playerId);
	CrossZoneReaper::RecordMigrationDone(playerId);
}
