#include "mission_event_receiver.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_base.h"

#include "event_proto/mission_event.pb.h"
///<<< END WRITING YOUR CODE

void MissionEventReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
}

static void UnRegister(entt::dispatcher& dispatcher){

dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
}
void MissionEventReceiver::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Accept(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiver::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Receive(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiver::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

