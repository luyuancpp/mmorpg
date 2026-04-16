#pragma once

#include "proto/common/event/actor_event.pb.h"

class ActorEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void InitializeActorCompsEventHandler(const InitializeActorCompsEvent& event);
    static void InterruptCurrentStateEventHandler(const InterruptCurrentStateEvent& event);
};
