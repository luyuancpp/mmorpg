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
    static void OnSceneCreateHandler(const OnSceneCreate& event);
    static void OnDestroySceneHandler(const OnDestroyScene& event);
    static void BeforeEnterSceneHandler(const BeforeEnterScene& event);
    static void AfterEnterSceneHandler(const AfterEnterScene& event);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& event);
    static void AfterLeaveSceneHandler(const AfterLeaveScene& event);
    static void S2CEnterSceneHandler(const S2CEnterScene& event);
};
