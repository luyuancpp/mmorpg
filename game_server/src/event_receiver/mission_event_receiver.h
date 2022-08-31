#pragma once
#include "src/game_logic/game_registry.h"

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;

class MissionEvent
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);

    static void Receive1(const AcceptMissionEvent& event_obj);
    static void Receive1(const MissionConditionEvent& event_obj);
    static void Receive1(const OnAcceptedMissionEvent& event_obj);
};
