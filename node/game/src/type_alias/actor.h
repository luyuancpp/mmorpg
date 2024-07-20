#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "proto/logic/client_player/scene_client_player.pb.h"

#define tls_actor_create_s2c tls.global_registry.get<ActorCreateS2C>(global_entity())
#define tls_actor_destroy_s2c tls.global_registry.get<ActorDestroyS2C>(global_entity())