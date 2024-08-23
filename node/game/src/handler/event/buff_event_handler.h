#pragma once

class SkillExecutedEvent;
class BeforeGiveDamageEvent;

class BuffEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void SkillExecutedEventHandler(const SkillExecutedEvent& event);
	static void BeforeGiveDamageEventHandler(const BeforeGiveDamageEvent& event);
};
