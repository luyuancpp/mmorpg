#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "scene/scene/comp/nav.h"

struct EnterSceneParam;
struct CreateSceneOnNodeSceneParam;
class OnSceneCreated;
class AfterEnterScene;
class BeforeLeaveScene;

class GameNodeSceneSystem
{
public:
    static void InitializeNodeScenes();
    static void HandleSceneCreation(const OnSceneCreated& message);
    static void HandleAfterEnterSceneEvent(const AfterEnterScene& message);
    static void HandleBeforeLeaveSceneEvent(const BeforeLeaveScene& message);
private:
};
