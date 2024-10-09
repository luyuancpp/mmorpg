#pragma once

class SkillExecutedEvent;
class BeforeGiveDamageEvent;
class AfterGiveDamageEvent;
class BeforeTakeDamageEvent;
class AfterTakeDamageEvent;
class BeforeDeadEvent;
class AfterDeadEvent;
class KillEvent;

class BuffEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void SkillExecutedEventHandler(const SkillExecutedEvent& event);
	static void BeforeGiveDamageEventHandler(const BeforeGiveDamageEvent& event);
	static void AfterGiveDamageEventHandler(const AfterGiveDamageEvent& event);
	static void BeforeTakeDamageEventHandler(const BeforeTakeDamageEvent& event);
	static void AfterTakeDamageEventHandler(const AfterTakeDamageEvent& event);
	static void BeforeDeadEventHandler(const BeforeDeadEvent& event);
	static void AfterDeadEventHandler(const AfterDeadEvent& event);
	static void KillEventHandler(const KillEvent& event);
};
