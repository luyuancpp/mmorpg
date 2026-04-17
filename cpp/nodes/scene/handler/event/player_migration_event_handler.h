#pragma once

#include "proto/common/event/player_migration_event.pb.h"

class PlayerMigrationEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void HandlePlayerMigrationEvent(const PlayerMigrationEvent& event);
};
