#include "player_migration_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerMigrationEventHandler::Register()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().connect<&PlayerMigrationEventHandler::OnPlayerMigrationEvent>();
}

void PlayerMigrationEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<PlayerMigrationEvent>().disconnect<&PlayerMigrationEventHandler::OnPlayerMigrationEvent>();
}
void PlayerMigrationEventHandler::OnPlayerMigrationEvent(const PlayerMigrationEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
