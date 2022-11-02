#pragma once
#include "event_receiver.h"
#include "scene_event_receiver.h"

void EventReceiverEvent::Register(entt::dispatcher& dispatcher)
{
    SceneEventReceiver::Register(dispatcher);
}

void EventReceiverEvent::UnRegister(entt::dispatcher& dispatcher)
{
    SceneEventReceiver::UnRegister(dispatcher);
}
