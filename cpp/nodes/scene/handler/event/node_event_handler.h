#pragma once

#include "proto/common/event/node_event.pb.h"

class NodeEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void OnNodeAddPbEventHandler(const OnNodeAddPbEvent& event);
    static void OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& event);
    static void ConnectToNodePbEventHandler(const ConnectToNodePbEvent& event);
    static void OnNodeConnectedPbEventHandler(const OnNodeConnectedPbEvent& event);
};
