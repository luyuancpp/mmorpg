#pragma once

class SkillExecutedEvent;

class BuffEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void SkillExecutedEventHandler(const SkillExecutedEvent& event);
};
