#pragma once
#include "src/game_logic/game_registry.h"

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;

class MissionEventReceiver
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);

    static void Receive0(const AcceptMissionEvent& event_obj);
    static void Receive1(const MissionConditionEvent& event_obj);
    static void Receive2(const OnAcceptedMissionEvent& event_obj);
};
