#pragma once
#include "src/game_logic/game_registry.h"

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;

class MissionEvent
{
public:
    void static void Register(entt::dispatcher& dispatcher);
    void static void UnRegister(entt::dispatcher& dispatcher);

    void static void Receive1(const AcceptMissionEvent& event_obj);
    void static void Receive1(const MissionConditionEvent& event_obj);
    void static void Receive1(const OnAcceptedMissionEvent& event_obj);
};
