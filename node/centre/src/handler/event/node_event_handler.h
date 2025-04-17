#pragma once

class OnSceneNodeAddPbEvent;

class NodeEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event);
};
