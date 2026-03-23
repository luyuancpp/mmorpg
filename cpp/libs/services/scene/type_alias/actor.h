#pragma once
#include <unordered_map>

#include "engine/core/type_define/type_define.h"
#include "engine/thread_context/registry_manager.h"
#include "engine/thread_context/entity_manager.h"
#include "proto/scene/player_scene.pb.h"

inline ActorCreateS2C& GetActorCreateMessage() {
	return tlsRegistryManager.globalRegistry.get<ActorCreateS2C>(GlobalEntity());
}

inline ActorDestroyS2C& GetActorDestroyMessage() {
	return tlsRegistryManager.globalRegistry.get<ActorDestroyS2C>(GlobalEntity());
}

inline ActorListCreateS2C& GetActorListCreateMessage() {
	return tlsRegistryManager.globalRegistry.get<ActorListCreateS2C>(GlobalEntity());
}

inline ActorListDestroyS2C& GetActorListDestroyMessage() {
	return tlsRegistryManager.globalRegistry.get<ActorListDestroyS2C>(GlobalEntity());
}
