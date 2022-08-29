#pragma once

class AcceptMissionEvent;
class MissionConditionEvent;
class OnAcceptedMissionEvent;

class MissionSystem
{
public:
	static void Receive1(const AcceptMissionEvent& event_obj);
	static void Receive2(const MissionConditionEvent& event_obj);
	static void Receive3(const OnAcceptedMissionEvent& event_obj);
};