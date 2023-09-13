#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

class PlayerSceneSystem
{
public:
    static void Send2GsEnterScene(entt::entity player);
    static void EnterSceneS2C(entt::entity player);
    static void CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id);
    static NodeId GetGameNodeIdByScene(entt::entity scene);
    static void TryEnterNextScene(entt::entity player);
};
