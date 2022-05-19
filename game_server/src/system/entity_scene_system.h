#pragma once

#include "entt/src/entt/entity/registry.hpp"

struct EnterSceneParam;

class EntitySceneSystem
{
public:
    void EnterScene(const EnterSceneParam& param);
    void OnEnterScene(entt::entity ent);
    void LeaveScene(entt::entity ent);
    void OnLeaveScene(entt::entity ent);
};

extern EntitySceneSystem g_entity_scene_system;