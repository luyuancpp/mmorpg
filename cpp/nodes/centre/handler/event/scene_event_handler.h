#pragma once
class OnRoomCreated;
class OnRoomDestroyed ;
class BeforeEnterScene;
class AfterEnterRoom;
class BeforeLeaveRoom;
class AfterLeaveScene;
class S2CEnterScene;

class SceneEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void OnSceneCreateHandler(const OnRoomCreated& event);
    static void OnDestroySceneHandler(const OnRoomDestroyed & event);
    static void BeforeEnterSceneHandler(const BeforeEnterScene& event);
    static void AfterEnterSceneHandler(const AfterEnterRoom& event);
    static void BeforeLeaveSceneHandler(const BeforeLeaveRoom& event);
    static void AfterLeaveSceneHandler(const AfterLeaveScene& event);
    static void S2CEnterSceneHandler(const S2CEnterScene& event);
};
