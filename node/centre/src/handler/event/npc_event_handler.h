#pragma once
class InitializeNpcComponentsEvent;

class NpcEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event);
};
