#pragma once

#include "proto/common/event/npc_event.pb.h"

class NpcEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event);
};
