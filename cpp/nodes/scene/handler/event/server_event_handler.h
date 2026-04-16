#pragma once

#include "proto/common/event/server_event.pb.h"

class ServerEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void OnConnect2CentreEventHandler(const OnConnect2CentreEvent& event);
    static void OnConnect2LoginHandler(const OnConnect2Login& event);
    static void OnServerStartHandler(const OnServerStart& event);
};
