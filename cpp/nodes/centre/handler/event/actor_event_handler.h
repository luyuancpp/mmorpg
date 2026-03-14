#pragma once

#include "proto/common/event/actor_event.pb.h"

class ActorEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void InitializeActorComponentsEventHandler(const InitializeActorComponentsEvent& event);
    static void InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event);
};
