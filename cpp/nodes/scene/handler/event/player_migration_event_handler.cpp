#include "player_migration_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerMigrationEventHandler::Register()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().connect<&PlayerMigrationEventHandler::PlayerMigrationEventHandler>();
}

void PlayerMigrationEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().disconnect<&PlayerMigrationEventHandler::PlayerMigrationEventHandler>();
}
void PlayerMigrationEventHandler::PlayerMigrationEventHandler(const PlayerMigrationEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
