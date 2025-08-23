#pragma once
class SkillExecutedEvent;

class SkillEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void SkillExecutedEventHandler(const SkillExecutedEvent& event);
};
