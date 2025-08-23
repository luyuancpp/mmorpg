#pragma once
class PlayerMigrationPbEvent;

class PlayerMigrationEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void PlayerMigrationPbEventHandler(const PlayerMigrationPbEvent& event);
};
