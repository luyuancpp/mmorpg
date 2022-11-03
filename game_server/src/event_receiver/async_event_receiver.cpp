#pragma once
#include "async_event_receiver.h"
#include "mission_async_event_receiver.h"
#include "scene_async_event_receiver.h"

void AsyncEventReceiverEvent::Register(entt::dispatcher& dispatcher)
{
    MissionAsyncEventReceiverReceiver::Register(dispatcher);
    SceneAsyncEventReceiverReceiver::Register(dispatcher);
}

void AsyncEventReceiverEvent::UnRegister(entt::dispatcher& dispatcher)
{
    MissionAsyncEventReceiverReceiver::UnRegister(dispatcher);
    SceneAsyncEventReceiverReceiver::UnRegister(dispatcher);
}
