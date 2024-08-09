#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "proto/logic/client_player/scene_player.pb.h"

#define actorCreateMessage tls.globalRegistry.get<ActorCreateS2C>(GlobalEntity())
#define actorDestroyMessage tls.globalRegistry.get<ActorDestroyS2C>(GlobalEntity())
#define actorListCreateMessage tls.globalRegistry.get<ActorListCreateS2C>(GlobalEntity())
#define actorListDestroyMessage tls.globalRegistry.get<ActorListDestroyS2C>(GlobalEntity())