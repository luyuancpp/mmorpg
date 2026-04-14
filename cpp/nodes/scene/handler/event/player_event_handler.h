#pragma once

#include "proto/common/event/player_event.pb.h"

class PlayerEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void RegisterPlayerEventHandler(const RegisterPlayerEvent& event);
    static void PlayerUpgradeEventHandler(const PlayerUpgradeEvent& event);
    static void InitializePlayerComponentsEventHandler(const InitializePlayerComponentsEvent& event);
    static void PlayerLoginEventHandler(const PlayerLoginEvent& event);
};
