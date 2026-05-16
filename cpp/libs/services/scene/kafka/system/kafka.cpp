#include "kafka.h"
#include "player/constants/player.h"
#include <muduo/base/Logging.h>
#include "player/system/player_lifecycle.h"
#include "proto/common/event/player_migration_event.pb.h"

// ⚠️ NOTE — Topic subscription wiring status (2026-05-16):
//
// This handler dispatches `player_migrate` and `player_migrate_ack` topics,
// but **neither topic is currently subscribed** by any scene-node start-up
// path. Verify with:
//   grep -rn "RegisterKafkaMessageHandler" cpp/  # only the engine impl + the
//                                                # SceneCommand command-handler
//                                                # template show up.
//
// The cross-zone migration code (player_lifecycle.cpp:HandleCrossZoneTransfer
// → KafkaProducer::send("player_migrate", ...)) publishes events nobody
// consumes. The audit doc cross-zone-readiness-audit.md §1 already captured
// this as part of the "cross-zone unfit for production" finding.
//
// Wiring options (to be picked in task #25 — ACK + reaper):
//   A) Add a Node::RegisterKafkaMessageHandler call in cpp/nodes/scene/main.cpp
//      that subscribes BOTH "player_migrate" and "player_migrate_ack" with
//      KafkaSystem::KafkaMessageHandler as the dispatch fn.
//   B) Wrap PlayerMigrationEvent / PlayerMigrationAckEvent inside SceneCommand
//      and ride the existing scene-{nodeId} command topic. Lower risk if the
//      project standardizes on SceneCommand for everything.
//
// I'm leaving this dispatcher in place because it's the right shape; the
// missing piece is the subscription side, not the message handling.
void KafkaSystem::KafkaMessageHandler(const std::string& topic, const std::string& message)
{
	if (topic == kPlayerMigrateEventName)
	{
		PlayerMigrationEvent serverEvent;
		if (serverEvent.ParseFromString(message))
		{
			PlayerLifecycleSystem::HandlePlayerMigration(serverEvent);
		}
		else
		{
			LOG_ERROR << "Failed to parse ServerEvent from message: " << message;
		}
	}
	else if (topic == "player_migrate_ack")
	{
		// Destination zone confirms it loaded the migrating player. Source-side
		// scene node clears PlayerFrozenComp and finally DestroyPlayer's the
		// source entity. See docs/design/cross-zone-readiness-audit.md §3.2
		// 件 2-3 for the full ACK protocol and §7 for failure scenarios.
		//
		// Payload is `PlayerMigrationAckEvent` (protobuf, same family as
		// PlayerMigrationEvent — keeps Kafka payload format consistent across
		// the codebase, no JSON parsing on the hot path).
		PlayerMigrationAckEvent ackEvent;
		if (!ackEvent.ParseFromString(message))
		{
			LOG_WARN << "[CrossZone] Failed to parse PlayerMigrationAckEvent (size="
					 << message.size() << ") — dropping; reaper will eventually retry.";
			return;
		}
		if (ackEvent.player_id() == 0)
		{
			LOG_WARN << "[CrossZone] PlayerMigrationAckEvent has player_id=0 — dropping.";
			return;
		}
		PlayerLifecycleSystem::HandlePlayerMigrationAck(
			static_cast<Guid>(ackEvent.player_id()),
			ackEvent.to_zone());
	}
	else
	{
		LOG_WARN << "Received unknown topic: " << topic;
	}
}

