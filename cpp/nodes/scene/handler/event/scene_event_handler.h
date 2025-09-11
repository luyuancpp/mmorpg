#pragma once
class OnRoomCreated;
class OnRoomDestroyed ;
class BeforeEnterRoom;
class AfterEnterRoom;
class BeforeLeaveRoom;
class AfterLeaveRoom;
class S2CEnterRoom;

class SceneEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void OnRoomCreateHandler(const OnRoomCreated& event);
    static void OnDestroyRoomHandler(const OnRoomDestroyed & event);
    static void BeforeEnterRoomHandler(const BeforeEnterRoom& event);
    static void AfterEnterRoomHandler(const AfterEnterRoom& event);
    static void BeforeRoomSceneHandler(const BeforeLeaveRoom& event);
    static void AfterRoomSceneHandler(const AfterLeaveRoom& event);
    static void S2CEnterRoomHandler(const S2CEnterRoom& event);
};
