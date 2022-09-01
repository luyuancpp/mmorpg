#pragma once
#include "event_receiver.h"
#include "mission_event_receiver.h"

void EventReceiverEvent::Register(entt::dispatcher& dispatcher)
{
    MissionEventReceiver::Register(dispatcher);
}

void EventReceiverEvent::UnRegister(entt::dispatcher& dispatcher)
{
    MissionEventReceiver::UnRegister(dispatcher);
}
