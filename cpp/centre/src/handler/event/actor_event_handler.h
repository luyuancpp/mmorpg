#pragma once
class InitializeActorComponentsEvent;
class InterruptCurrentStatePbEvent;

class ActorEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void InitializeActorComponentsEventHandler(const InitializeActorComponentsEvent& event);
    static void InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event);
};
