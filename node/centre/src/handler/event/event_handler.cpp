#include "event_handler.h"
#include "actor_combat_state_event_handler.h"
#include "actor_event_handler.h"
#include "buff_event_handler.h"
#include "combat_event_handler.h"
#include "mission_event_handler.h"
#include "npc_event_handler.h"
#include "player_event_handler.h"
#include "scene_event_handler.h"
#include "server_event_handler.h"
#include "skill_event_handler.h"
void EventHandler::Register()
{
ActorCombatStateEventHandler::Register();
ActorEventHandler::Register();
BuffEventHandler::Register();
CombatEventHandler::Register();
MissionEventHandler::Register();
NpcEventHandler::Register();
PlayerEventHandler::Register();
SceneEventHandler::Register();
ServerEventHandler::Register();
SkillEventHandler::Register();
}
void EventHandler::UnRegister()
{
ActorCombatStateEventHandler::UnRegister();
ActorEventHandler::UnRegister();
BuffEventHandler::UnRegister();
CombatEventHandler::UnRegister();
MissionEventHandler::UnRegister();
NpcEventHandler::UnRegister();
PlayerEventHandler::UnRegister();
SceneEventHandler::UnRegister();
ServerEventHandler::UnRegister();
SkillEventHandler::UnRegister();
}
