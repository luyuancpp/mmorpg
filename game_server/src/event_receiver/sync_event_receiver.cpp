#pragma once
#include "sync_event_receiver.h"
#include "mission_event_sync_event_receiver.h"
#include "scene_event_sync_event_receiver.h"

void SyncEventReceiverEvent::Register(entt::dispatcher& dispatcher)
{
    MissionEventReceiver::Register(dispatcher);
    SceneEventReceiver::Register(dispatcher);
}

void SyncEventReceiverEvent::UnRegister(entt::dispatcher& dispatcher)
{
    MissionEventReceiver::UnRegister(dispatcher);
    SceneEventReceiver::UnRegister(dispatcher);
}
