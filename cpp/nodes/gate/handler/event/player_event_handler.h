#pragma once

#include "proto/contracts/kafka/player_event.pb.h"

class PlayerEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void PlayerLifecycleCommandHandler(const contracts::kafka::PlayerLifecycleCommand& event);
};
