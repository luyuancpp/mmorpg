#pragma once

#include "proto/common/event/combat_event.pb.h"

class CombatEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void BeKillEventHandler(const BeKillEvent& event);
};
