#pragma once

#include "core/utils/registry/game_registry.h"
#include "proto/common/base/node.pb.h"
#include "engine/core/type_define/type_define.h"
#include <cassert>  // 用于边界检查

// 节点类型数量（建议从枚举定义中获取，此处为示例）
constexpr uint32_t kNodeTypeCount = eNodeType_ARRAYSIZE;

// 存储每种节点类型的ECS注册表
using NodeRegistries = std::array<entt::registry, kNodeTypeCount>;
// 存储每种节点类型的全局实体
using NodeGlobalEntity = std::array<entt::entity, kNodeTypeCount>;

/**
 * 线程局部的节点上下文管理类
 * 每个线程拥有独立的实例，管理该线程内不同节点类型的ECS注册表和全局实体
 */
class NodeContextManager {
public:
    NodeContextManager() = default;
    // 禁止拷贝和移动，确保单例唯一性
    NodeContextManager(const NodeContextManager&) = delete;
    NodeContextManager& operator=(const NodeContextManager&) = delete;

    /**
     * 获取指定节点类型的ECS注册表
     * @param nodeType 节点类型（需在 [0, kNodeTypeCount) 范围内）
     * @return 对应节点类型的 entt::registry 引用
     */
    entt::registry& GetRegistry(uint32_t nodeType) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        return registries_[nodeType];
    }

    /**
     * 获取所有节点类型的注册表数组
     * @return 包含所有注册表的 NodeRegistries 引用
     */
    NodeRegistries& GetAllRegistries() {
        return registries_;
    }

    /**
     * 设置指定节点类型的全局实体
     * @param nodeType 节点类型（需在 [0, kNodeTypeCount) 范围内）
     * @param entity 要设置的全局实体
     */
    void SetGlobalEntity(uint32_t nodeType, entt::entity entity) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        globalEntities_[nodeType] = entity;
    }

    /**
     * 获取指定节点类型的全局实体（不自动创建）
     * @param nodeType 节点类型（需在 [0, kNodeTypeCount) 范围内）
     * @return 对应的全局实体，若未设置则返回 entt::null
     */
    entt::entity GetGlobalEntity(uint32_t nodeType) const {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        return globalEntities_[nodeType];
    }

    /**
     * 获取指定节点类型的全局实体（若不存在则自动创建）
     * @param nodeType 节点类型（需在 [0, kNodeTypeCount) 范围内）
     * @return 对应的全局实体（已创建或已存在）
     */
    entt::entity GetOrCreateGlobalEntity(uint32_t nodeType) {
        assert(nodeType < kNodeTypeCount && "nodeType out of range");
        if (globalEntities_[nodeType] == entt::null) {
            // 自动创建实体并关联到对应注册表
            globalEntities_[nodeType] = GetRegistry(nodeType).create();
        }
        return globalEntities_[nodeType];
    }

    /**
     * 清理所有节点类型的注册表和全局实体
     */
    void Clear() {
        for (auto& registry : registries_) {
            registry.clear();  // 清空注册表内所有实体和组件
        }
        globalEntities_.fill(entt::null);  // 重置所有全局实体为无效
    }

private:
    NodeRegistries registries_;         // 每种节点类型的ECS注册表
    NodeGlobalEntity globalEntities_;   // 每种节点类型的全局实体
};

extern thread_local NodeContextManager tlsNodeContextManager;
