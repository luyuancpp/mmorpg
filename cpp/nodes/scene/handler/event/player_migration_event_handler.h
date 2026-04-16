#pragma once

#include "proto/common/event/player_migration_event.pb.h"

class PlayerMigrationEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void OnPlayerMigrationEvent(const PlayerMigrationEvent& event);
};
