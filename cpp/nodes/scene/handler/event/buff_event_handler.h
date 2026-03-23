#pragma once

#include "proto/common/event/buff_event.pb.h"

class BuffEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void BuffTestEventHandler(const BuffTestEvent& event);
};
