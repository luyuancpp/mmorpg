#include "event_handler.h"

#include "battle_event_handler.h"
#include "player_migration_event_handler.h"
#include "team_event_handler.h"

void RegisterNodeEvents() { EventHandler::Register(); }

void EventHandler::Register()
{
BattleEventHandler::Register();
PlayerMigrationEventHandler::Register();
TeamEventHandler::Register();

}

void EventHandler::UnRegister()
{
BattleEventHandler::UnRegister();
PlayerMigrationEventHandler::UnRegister();
TeamEventHandler::UnRegister();

}