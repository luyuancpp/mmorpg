#pragma once

class OnRoomCreated;
class AfterEnterRoom;
class BeforeLeaveRoom;

class SceneCrowdSystem
{
public:
    static void AfterEnterSceneHandler(const AfterEnterRoom& message);
    static void BeforeLeaveSceneHandler(const BeforeLeaveRoom& message);
};