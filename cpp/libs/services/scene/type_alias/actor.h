#pragma once
#include <unordered_map>

#include "engine/core/type_define/type_define.h"
#include "engine/thread_context/registry_manager.h"
#include "engine/thread_context/entity_manager.h"
#include "proto/scene/player_scene.pb.h"

inline ActorCreateS2C& GetActorCreateMessage() {
	return tlsEcs.globalRegistry.get<ActorCreateS2C>(tlsEcs.GlobalEntity());
}

inline ActorDestroyS2C& GetActorDestroyMessage() {
	return tlsEcs.globalRegistry.get<ActorDestroyS2C>(tlsEcs.GlobalEntity());
}

inline ActorListCreateS2C& GetActorListCreateMessage() {
	return tlsEcs.globalRegistry.get<ActorListCreateS2C>(tlsEcs.GlobalEntity());
}

inline ActorListDestroyS2C& GetActorListDestroyMessage() {
	return tlsEcs.globalRegistry.get<ActorListDestroyS2C>(tlsEcs.GlobalEntity());
}
