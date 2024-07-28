#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "proto/logic/client_player/scene_client_player.pb.h"

#define actorCreateMessage tls.globalRegistry.get<ActorCreateS2C>(global_entity())
#define actorDestroyMessage tls.globalRegistry.get<ActorDestroyS2C>(global_entity())
#define actorListCreateMessage tls.globalRegistry.get<ActorListCreateS2C>(global_entity())
#define actorListDestroyMessage tls.globalRegistry.get<ActorListDestroyS2C>(global_entity())