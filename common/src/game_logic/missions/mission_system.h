#pragma once

class AcceptMissionEvent;
class MissionConditionEvent;

class MissionSystem
{
public:
	static void Receive1(const AcceptMissionEvent& ev);
	static void Receive2(const MissionConditionEvent& ev);
};