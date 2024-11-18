#pragma once

class SilenceAddedPbEvent;
class SilenceRemovedEvent;

class ActorCombatStateEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void SilenceAddedPbEventHandler(const SilenceAddedPbEvent& event);
	static void SilenceRemovedEventHandler(const SilenceRemovedEvent& event);
};
