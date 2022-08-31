#include "mission_system.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_base.h"

#include "event_proto/mission_event.pb.h"

void MissionEventReceiver::Receive1(const AcceptMissionEvent& event_obj)
{
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Accept(event_obj);
}

void MissionEventReceiver::Receive2(const MissionConditionEvent& event_obj)
{
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Receive(event_obj);
}

void MissionEventReceiver::Receive3(const OnAcceptedMissionEvent& event_obj)
{

}
