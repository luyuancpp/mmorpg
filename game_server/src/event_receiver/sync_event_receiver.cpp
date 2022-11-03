#pragma once
#include "sync_event_receiver.h"
#include "mission_sync_event_receiver.h"
#include "scene_sync_event_receiver.h"

void SyncEventReceiverEvent::Register(entt::dispatcher& dispatcher)
{
    MissionSyncEventReceiverReceiver::Register(dispatcher);
    SceneSyncEventReceiverReceiver::Register(dispatcher);
}

void SyncEventReceiverEvent::UnRegister(entt::dispatcher& dispatcher)
{
    MissionSyncEventReceiverReceiver::UnRegister(dispatcher);
    SceneSyncEventReceiverReceiver::UnRegister(dispatcher);
}
