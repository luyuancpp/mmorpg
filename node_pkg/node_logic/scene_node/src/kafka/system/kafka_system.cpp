#include "kafka_system.h"
#include "player/constants/player_constants.h"
#include <muduo/base/Logging.h>
#include "player/system/player_node_system.h"
#include "proto/logic/event/player_migration_event.pb.h"

void KafkaSystem::KafkaMessageHandler(const std::string& topic, const std::string& message)
{
	if (topic == kPlayerMirgrateEventName)
	{
		PlayerMigrationPbEvent serverEvent;
		if (serverEvent.ParseFromString(message))
		{
			PlayerNodeSystem::HandlePlayerMigration(serverEvent);
		}
		else
		{
			LOG_ERROR << "Failed to parse ServerEvent from message: " << message;
		}
	}
	else
	{
		LOG_WARN << "Received unknown topic: " << topic;
	}
}

