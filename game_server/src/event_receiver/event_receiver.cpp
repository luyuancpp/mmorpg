#pragma once
#include "event_receiver.h"
#include "mission_event_receiver.h"
#include "scene_event_receiver.h"

void EventReceiver::Register(entt::dispatcher& dispatcher)
{
    MissionEventReceiverReceiver::Register(dispatcher);
    SceneEventReceiverReceiver::Register(dispatcher);
}

void EventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
    MissionEventReceiverReceiver::UnRegister(dispatcher);
    SceneEventReceiverReceiver::UnRegister(dispatcher);
}
