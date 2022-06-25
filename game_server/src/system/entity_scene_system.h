#pragma once

#include "entt/src/entt/entity/registry.hpp"

struct EnterSceneParam;

class EntitySceneSystem
{
public:
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
};
