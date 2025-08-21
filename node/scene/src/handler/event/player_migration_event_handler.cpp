#include "player_migration_event_handler.h"
#include "proto/logic/event/player_migration_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void PlayerMigrationEventHandler::Register()
{
    dispatcher.sink<PlayerMigrationPbEvent>().connect<&PlayerMigrationEventHandler::PlayerMigrationPbEventHandler>();
}

void PlayerMigrationEventHandler::UnRegister()
{
    dispatcher.sink<PlayerMigrationPbEvent>().disconnect<&PlayerMigrationEventHandler::PlayerMigrationPbEventHandler>();
}
void PlayerMigrationEventHandler::PlayerMigrationPbEventHandler(const PlayerMigrationPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
