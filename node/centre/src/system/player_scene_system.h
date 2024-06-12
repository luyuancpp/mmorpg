#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "type_define/type_define.h"

class PlayerSceneSystem
{
public:
    static void Send2GsEnterScene(entt::entity player);
    static void EnterSceneS2C(entt::entity player);
    static void CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id);
    static void TryEnterNextScene(entt::entity player);
};
