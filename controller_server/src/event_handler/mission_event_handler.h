#pragma once
#include "src/game_logic/thread_local/thread_local_storage.h"

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;
class OnMissionAwardEvent;

class MissionEventHandler
{
	static void Register(entt::dispatcher& dispatcher);
	static void UnRegister(entt::dispatcher& dispatcher);

	static void AcceptMissionEventHandler(const AcceptMissionEvent& message);
	static void MissionConditionEventHandler(const MissionConditionEvent& message);
	static void OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& message);
	static void OnMissionAwardEventHandler(const OnMissionAwardEvent& message);
};
