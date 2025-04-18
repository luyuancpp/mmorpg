#pragma once
class BeKillEvent;

class CombatEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void BeKillEventHandler(const BeKillEvent& event);
};
