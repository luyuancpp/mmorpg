#pragma once

class SilenceAddedPbEvent;
class SilenceRemovedPbEvent;

class ActorCombatStateEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void SilenceAddedPbEventHandler(const SilenceAddedPbEvent& event);
	static void SilenceRemovedPbEventHandler(const SilenceRemovedPbEvent& event);
};
