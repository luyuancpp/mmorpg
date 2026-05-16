#include "player_migration_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerMigrationEventHandler::Register()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().connect<&PlayerMigrationEventHandler::HandlePlayerMigrationEvent>();
    tlsEcs.dispatcher.sink<PlayerMigrationAckEvent>().connect<&PlayerMigrationEventHandler::PlayerMigrationAckEventHandler>();
}

void PlayerMigrationEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().disconnect<&PlayerMigrationEventHandler::HandlePlayerMigrationEvent>();
    tlsEcs.dispatcher.sink<PlayerMigrationAckEvent>().disconnect<&PlayerMigrationEventHandler::PlayerMigrationAckEventHandler>();
}
void PlayerMigrationEventHandler::HandlePlayerMigrationEvent(const PlayerMigrationEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void PlayerMigrationEventHandler::PlayerMigrationAckEventHandler(const PlayerMigrationAckEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
