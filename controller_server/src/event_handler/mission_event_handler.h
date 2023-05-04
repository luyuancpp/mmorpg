#pragma once
#include "src/game_logic/thread_local/thread_local_storage.h"

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;
class OnMissionAwardEvent;

class MissionEventHandler
{
public:
	static void Register(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
		dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
		dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
	}

	static void UnRegister(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
		dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
		dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
	}

	static void AcceptMissionEventHandler(const AcceptMissionEvent& message);
	static void MissionConditionEventHandler(const MissionConditionEvent& message);
	static void OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& message);
	static void OnMissionAwardEventHandler(const OnMissionAwardEvent& message);
};
