#pragma once
class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;
class OnMissionAwardEvent;

class MissionEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void AcceptMissionEventHandler(const AcceptMissionEvent& event);
    static void MissionConditionEventHandler(const MissionConditionEvent& event);
    static void OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& event);
    static void OnMissionAwardEventHandler(const OnMissionAwardEvent& event);
};
