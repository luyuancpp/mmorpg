#pragma once
#include <unordered_map>

#include "base/common/type_define/type_define.h"
#include "base/threading/entity_manager.h"
#include "proto/scene/player_scene.pb.h"

#define actorCreateMessage tlsRegistryManager.globalRegistry.get<ActorCreateS2C>(GlobalEntity())
#define actorDestroyMessage tlsRegistryManager.globalRegistry.get<ActorDestroyS2C>(GlobalEntity())
#define actorListCreateMessage tlsRegistryManager.globalRegistry.get<ActorListCreateS2C>(GlobalEntity())
#define actorListDestroyMessage tlsRegistryManager.globalRegistry.get<ActorListDestroyS2C>(GlobalEntity())