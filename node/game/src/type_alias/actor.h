#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "proto/logic/client_player/scene_client_player.pb.h"

#define actorCreateMessage tls.globalRegistry.get<ActorCreateS2C>(global_entity())
#define actorDestroyMessage tls.globalRegistry.get<ActorDestroyS2C>(global_entity())
#define mainActorCreateMessage tls.globalRegistry.get<MainActorCreateS2C>(global_entity())
#define mainActorDestroyMessage tls.globalRegistry.get<MainActorDestroyS2C>(global_entity())