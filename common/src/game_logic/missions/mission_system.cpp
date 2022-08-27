#include "mission_system.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_base.h"

#include "event_proto/mission_event.pb.h"

void MissionSystem::Receive1(const AcceptMissionEvent& ev)
{
	auto entity = entt::to_entity(ev.entity());
	registry.get<MissionsComp>(entity).Accept(ev);
}

void MissionSystem::Receive2(const MissionConditionEvent& ev)
{
	//auto entity = entt::to_entity(ev.entity());
	//registry.get<MissionsComp>(entity).Receive(ev);
}

