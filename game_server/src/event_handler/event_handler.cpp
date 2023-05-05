#include "event_handler.h"
#include "mission_event_handler.h"
#include "scene_event_handler.h"
void EventHandler::Register(entt::dispatcher& dispatcher)
{
MissionEventHandler::Register(dispatcher);
SceneEventHandler::Register(dispatcher);
}
void EventHandler::UnRegister(entt::dispatcher& dispatcher)
{
MissionEventHandler::UnRegister(dispatcher);
SceneEventHandler::UnRegister(dispatcher);
}
