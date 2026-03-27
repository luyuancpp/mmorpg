#include "player_migration_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerMigrationEventHandler::Register()
{
    tlsEcs.dispatcher.sink<PlayerMigrationPbEvent>().connect<&PlayerMigrationEventHandler::PlayerMigrationPbEventHandler>();
}

void PlayerMigrationEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<PlayerMigrationPbEvent>().disconnect<&PlayerMigrationEventHandler::PlayerMigrationPbEventHandler>();
}
void PlayerMigrationEventHandler::PlayerMigrationPbEventHandler(const PlayerMigrationPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
