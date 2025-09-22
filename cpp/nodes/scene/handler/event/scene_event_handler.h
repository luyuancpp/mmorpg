#pragma once
class OnRoomCreated;
class OnRoomDestroyed;
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
    static void OnRoomCreatedHandler(const OnRoomCreated& event);
    static void OnRoomDestroyedHandler(const OnRoomDestroyed& event);
    static void BeforeEnterRoomHandler(const BeforeEnterRoom& event);
    static void AfterEnterRoomHandler(const AfterEnterRoom& event);
    static void BeforeLeaveRoomHandler(const BeforeLeaveRoom& event);
    static void AfterLeaveRoomHandler(const AfterLeaveRoom& event);
    static void S2CEnterRoomHandler(const S2CEnterRoom& event);
};
