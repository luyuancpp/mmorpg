#pragma once

class OnSceneCreate;
class OnDestroyScene;
class BeforeEnterScene;
class AfterEnterScene;
class BeforeLeaveScene;
class AfterLeaveScene;
class S2CEnterScene;

class SceneEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void OnSceneCreateHandler(const OnSceneCreate& message);
	static void OnDestroySceneHandler(const OnDestroyScene& message);
	static void BeforeEnterSceneHandler(const BeforeEnterScene& message);
	static void AfterEnterSceneHandler(const AfterEnterScene& message);
	static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
	static void AfterLeaveSceneHandler(const AfterLeaveScene& message);
	static void S2CEnterSceneHandler(const S2CEnterScene& message);
};
