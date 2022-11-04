#pragma once
#include "src/game_logic/game_registry.h"

class BeforeEnterScene;
class OnEnterScene;
class BeforeLeaveScene;
class OnLeaveScene;

class SceneEventReceiverReceiver
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);

    static void Receive0(const BeforeEnterScene& event_obj);
    static void Receive1(const OnEnterScene& event_obj);
    static void Receive2(const BeforeLeaveScene& event_obj);
    static void Receive3(const OnLeaveScene& event_obj);
};
