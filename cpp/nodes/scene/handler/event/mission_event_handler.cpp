#include "mission_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/system/mission.h"
#include <thread_context/registry_manager.h>

///<<< END WRITING YOUR CODE
void MissionEventHandler::Register()
{
    dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
    dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
    dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::UnRegister()
{
    dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
    dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
    dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
}
void MissionEventHandler::AcceptMissionEventHandler(const AcceptMissionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity entity = entt::to_entity(event.entity());
    auto& container = tlsRegistryManager.actorRegistry.get_or_emplace<MissionsContainerComponent>(entity);
    auto& comp = container.GetOrCreate(MissionListPBComponent::kPlayerMission);
	MissionSystem::AcceptMission(event, comp);
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity entity = entt::to_entity(event.entity());
	auto& container = tlsRegistryManager.actorRegistry.get_or_emplace<MissionsContainerComponent>(entity);
	auto& comp = container.GetOrCreate(MissionListPBComponent::kPlayerMission);
    MissionSystem::HandleMissionConditionEvent(event, comp);
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
