#pragma once
class CombatStateAddedPbEvent;
class CombatStateRemovedPbEvent;

class ActorCombatStateEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void CombatStateAddedPbEventHandler(const CombatStateAddedPbEvent& event);
    static void CombatStateRemovedPbEventHandler(const CombatStateRemovedPbEvent& event);
};
