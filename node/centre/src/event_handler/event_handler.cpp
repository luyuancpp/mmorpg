#include "event_handler.h"
#include "mission_event_handler.h"
#include "scene_event_handler.h"
void EventHandler::Register()
{
MissionEventHandler::Register();
SceneEventHandler::Register();
}
void EventHandler::UnRegister()
{
MissionEventHandler::UnRegister();
SceneEventHandler::UnRegister();
}
