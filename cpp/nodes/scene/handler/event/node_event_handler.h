#pragma once

#include "proto/common/event/node_event.pb.h"

class NodeEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void OnNodeAddEventHandler(const OnNodeAddEvent& event);
    static void OnNodeRemoveEventHandler(const OnNodeRemoveEvent& event);
    static void ConnectToNodeEventHandler(const ConnectToNodeEvent& event);
    static void OnNodeConnectedEventHandler(const OnNodeConnectedEvent& event);
};
