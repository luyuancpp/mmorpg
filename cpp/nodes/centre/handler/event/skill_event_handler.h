#pragma once

#include "proto/common/event/skill_event.pb.h"

class SkillEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void SkillExecutedEventHandler(const SkillExecutedEvent& event);
};
