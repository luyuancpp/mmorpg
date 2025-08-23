#pragma once
class BuffTestEvet;

class BuffEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void BuffTestEvetHandler(const BuffTestEvet& event);
};
