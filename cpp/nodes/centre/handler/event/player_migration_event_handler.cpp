#include "player_migration_event_handler.h"
#include "threading/dispatcher_manager.h"

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

}
