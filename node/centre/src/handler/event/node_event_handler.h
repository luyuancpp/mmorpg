#pragma once
class OnSceneNodeAddPbEvent;
class OnSceneNodeRemovePbEvent;

class NodeEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event);
    static void OnSceneNodeRemovePbEventHandler(const OnSceneNodeRemovePbEvent& event);
};
