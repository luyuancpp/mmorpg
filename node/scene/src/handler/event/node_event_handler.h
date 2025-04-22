#pragma once
class OnSceneNodeAddPbEvent;
class OnSceneNodeRemovePbEvent;
class OnNodeAddPbEvent;
class OnNodeRemovePbEvent;
class ConnectToNodePbEvent;
class OnNodeConnectedPbEvent;

class NodeEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event);
    static void OnSceneNodeRemovePbEventHandler(const OnSceneNodeRemovePbEvent& event);
    static void OnNodeAddPbEventHandler(const OnNodeAddPbEvent& event);
    static void OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& event);
    static void ConnectToNodePbEventHandler(const ConnectToNodePbEvent& event);
    static void OnNodeConnectedPbEventHandler(const OnNodeConnectedPbEvent& event);
};
