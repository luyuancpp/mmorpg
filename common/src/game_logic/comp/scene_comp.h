#pragma once

#include <memory>

#include "src/game_logic/thread_local/thread_local_storage.h"

#include "src/common_type/common_type.h"



using SceneList = std::unordered_map<Guid, entt::entity>;
using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
using ScenePlayers = EntitySet;//弱引用，要解除玩家和场景的耦合






