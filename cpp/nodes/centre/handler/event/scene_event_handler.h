#pragma once

#include "proto/common/event/scene_event.pb.h"

class SceneEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void OnSceneCreatedHandler(const OnSceneCreated& event);
    static void OnSceneDestroyedHandler(const OnSceneDestroyed& event);
    static void BeforeEnterSceneHandler(const BeforeEnterScene& event);
    static void AfterEnterSceneHandler(const AfterEnterScene& event);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& event);
    static void AfterLeaveSceneHandler(const AfterLeaveScene& event);
    static void S2CEnterSceneHandler(const S2CEnterScene& event);
};
