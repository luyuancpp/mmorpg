#pragma once
#include "node/system/node/node_util.h"
#include "engine/core/type_define/type_define.h"

using ServiceNodeList = std::array<NodeInfoListComp, eNodeType_ARRAYSIZE>;

NodeInfo &GetNodeInfo();
uint32_t GetZoneId();

// Pick a SceneManager node by player_id hash. Returns entt::null if none available.
entt::entity GetSceneManagerEntity(Guid playerId);
