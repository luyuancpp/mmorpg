#pragma once

#include "core/utils/registry/game_registry.h"
#include "node/system/node/node_util.h"
#include "engine/core/type_define/type_define.h"
#include <cassert>

constexpr uint32_t kNodeTypeCount = eNodeType_ARRAYSIZE;

using NodeRegistries = std::array<entt::registry, kNodeTypeCount>;
using NodeGlobalEntity = std::array<entt::entity, kNodeTypeCount>;

// Thread-local manager for per-node-type ECS registries and global entities.
class NodeContextManager {
public:
    NodeContextManager() = default;
    NodeContextManager(const NodeContextManager&) = delete;
    NodeContextManager& operator=(const NodeContextManager&) = delete;

    entt::registry& GetRegistry(uint32_t nodeType) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        return registries_[nodeType];
    }

    NodeRegistries& GetAllRegistries() {
        return registries_;
    }

    void SetGlobalEntity(uint32_t nodeType, entt::entity entity) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        globalEntities_[nodeType] = entity;
    }

    entt::entity GetGlobalEntity(uint32_t nodeType) const {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        return globalEntities_[nodeType];
    }

    // Returns existing or creates new global entity for this node type.
    entt::entity GetOrCreateGlobalEntity(uint32_t nodeType) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        if (globalEntities_[nodeType] == entt::null) {
            globalEntities_[nodeType] = GetRegistry(nodeType).create();
        }
        return globalEntities_[nodeType];
    }

    void Clear() {
        for (auto& registry : registries_) {
            registry.clear();
        }
        globalEntities_.fill(entt::null);
    }

private:
    NodeRegistries registries_;
    NodeGlobalEntity globalEntities_;
};

extern thread_local NodeContextManager tlsNodeContextManager;
