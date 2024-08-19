#pragma once

class AbilityExecutedEvent;

class BuffEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void AbilityExecutedEventHandler(const AbilityExecutedEvent& event);
};
