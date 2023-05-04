#pragma once
#include "event_receiver.h"
#include "mission_event_receiver.h"
#include "scene_event_receiver.h"

void EventReceiver::Register(entt::dispatcher& dispatcher)
{
    MissionEventHandler::Register(dispatcher);
    SceneEventHandler::Register(dispatcher);
}

void EventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
    MissionEventHandler::UnRegister(dispatcher);
    SceneEventHandler::UnRegister(dispatcher);
}
