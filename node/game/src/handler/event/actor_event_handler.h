#pragma once

class InterruptCurrentStatePbEvent;

class ActorEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event);
};
