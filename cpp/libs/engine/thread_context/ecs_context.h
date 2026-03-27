#pragma once

#include <unordered_map>
#include "core/utils/registry/game_registry.h"
#include "entt/src/entt/signal/dispatcher.hpp"
#include "core/type_define/type_define.h"

using PlayerListMap = std::unordered_map<Guid, entt::entity>;

// Consolidated ECS world state — one thread_local instance per process.
// Groups all entity registries, well-known entities, player index, and the
// event dispatcher that were previously scattered across 6 separate files.
//
// Lifetime: process-level (survives across RPC calls).
struct EcsContext
{
    // --- Entity Registries ---
    entt::registry globalRegistry;
    entt::registry actorRegistry;
    entt::registry sceneRegistry;
    entt::registry itemRegistry;
    entt::registry nodeGlobalRegistry;

    // --- Event Dispatcher ---
    entt::dispatcher dispatcher;

    // --- Player Index ---
    PlayerListMap playerList;

    // --- Well-known Entity Getters (lazy-initialized) ---
    entt::entity GlobalEntity();
    entt::entity ErrorEntity();
    entt::entity OperatorEntity();
    entt::entity GrpcNodeEntity();

    // --- Player Lookup ---
    entt::entity GetPlayer(Guid playerUid);

    // --- Lifecycle ---
    void Clear();

private:
    entt::entity globalEntity_{ entt::null };
    entt::entity errorEntity_{ entt::null };
    entt::entity operatorEntity_{ entt::null };
    entt::entity grpcNodeEntity_{ entt::null };
};

extern thread_local EcsContext tlsEcs;
