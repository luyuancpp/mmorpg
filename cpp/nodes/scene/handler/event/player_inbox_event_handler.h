#pragma once

#include "proto/common/event/player_inbox_event.pb.h"

class PlayerInboxEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void PlayerInboxNotifyEventHandler(const PlayerInboxNotifyEvent& event);
};
