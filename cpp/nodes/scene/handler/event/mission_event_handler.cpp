#include "mission_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/system/mission.h"
#include "modules/mission/comp/missions_config_comp.h"
#include <thread_context/registry_manager.h>

///<<< END WRITING YOUR CODE
void MissionEventHandler::Register()
{
    tlsEcs.dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
    tlsEcs.dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
    tlsEcs.dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    tlsEcs.dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
    tlsEcs.dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
    tlsEcs.dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    tlsEcs.dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
}
void MissionEventHandler::AcceptMissionEventHandler(const AcceptMissionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity entity = entt::to_entity(event.entity());
    auto& container = tlsEcs.actorRegistry.get_or_emplace<MissionsContainerComp>(entity);
    auto& comp = container.GetOrCreate(MissionListComp::kPlayerMission);
	MissionSystem::AcceptMission(event, comp, MissionConfig::GetSingleton());
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity entity = entt::to_entity(event.entity());
	auto& container = tlsEcs.actorRegistry.get_or_emplace<MissionsContainerComp>(entity);
	auto& comp = container.GetOrCreate(MissionListComp::kPlayerMission);
    MissionSystem::HandleMissionConditionEvent(event, comp, MissionConfig::GetSingleton());
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
     // Trigger auto-matching of current mission progress on accept (e.g., update gold-refresh quest counters)
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::OnMissionAwardEventHandler(const OnMissionAwardEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
