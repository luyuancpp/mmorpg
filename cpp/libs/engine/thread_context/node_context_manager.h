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
    // globalEntities_ 必须显式填成 entt::null:std::array<entt::entity, N>
    // 默认构造不初始化元素,thread_local 的零初始化残留是 entity{0} ——
    // 而 64 位 entt 下 entt::null 是全 1,不是 0。此前
    // GetOrCreateGlobalEntity 的懒创建判断(== entt::null)因此永远不成立,
    // GetGlobalEntity 返回的是一个从未 create() 过的幽灵句柄,etcd 全部
    // gRPC 客户端状态(CQ/stub/watch 流)都 emplace 在它上面,仅靠
    // EnTT 池语义宽容才"碰巧能跑"。同目录 EcsContext 对各 globalEntity_
    // 逐一 { entt::null } 初始化,这里此前漏了。
    NodeContextManager() { globalEntities_.fill(entt::null); }
    NodeContextManager(const NodeContextManager&) = delete;
    NodeContextManager& operator=(const NodeContextManager&) = delete;

    [[nodiscard]] static constexpr bool IsValidNodeType(uint32_t nodeType) {
        return nodeType < kNodeTypeCount;
    }

    // 只给可信的内部调用方:nodeType 是编译期常量,或本进程已经校验过的值。
    // 越界是编程错误,任何构建下都 LOG_FATAL 终止(fail-fast,与 RpcController::From 同口径)。
    // 以前只有 assert,Release 下 NDEBUG 把它抹掉,越界直接读写 registries_ 之外的内存
    // (ecs-entt-misuse-audit-20261015 F093/C007)。
    // 不返回替身 registry 让调用方照常往下跑:那样写路径(建 RpcClient、发起连接)的副作用照样发生,
    // 只是落进一个谁也不查的地方,还会让不同的非法类型互相查到对方写进去的实体,比当场终止更难发现。
    // 来自对端请求、etcd、Kafka 等外部输入的 nodeType 不要走这里,先用 TryGetRegistry 显式拒绝。
    entt::registry& GetRegistry(uint32_t nodeType) {
        RequireValidNodeType("GetRegistry", nodeType);
        return registries_[nodeType];
    }

    // 给拿外部输入选 registry 的入口用:越界返回 nullptr,由调用方 LOG_ERROR 并拒绝本次请求。
    [[nodiscard]] entt::registry* TryGetRegistry(uint32_t nodeType) {
        return IsValidNodeType(nodeType) ? &registries_[nodeType] : nullptr;
    }

    NodeRegistries& GetAllRegistries() {
        return registries_;
    }

    // 下面三个与 GetRegistry 同一前置条件:只给可信内部调用方,越界即 LOG_FATAL,
    // 不再在 Release 下越界读写 globalEntities_。
    void SetGlobalEntity(uint32_t nodeType, entt::entity entity) {
        RequireValidNodeType("SetGlobalEntity", nodeType);
        globalEntities_[nodeType] = entity;
    }

    entt::entity GetGlobalEntity(uint32_t nodeType) const {
        RequireValidNodeType("GetGlobalEntity", nodeType);
        return globalEntities_[nodeType];
    }

    // Returns existing or creates new global entity for this node type.
    entt::entity GetOrCreateGlobalEntity(uint32_t nodeType) {
        RequireValidNodeType("GetOrCreateGlobalEntity", nodeType);
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
    static void RequireValidNodeType(const char* caller, uint32_t nodeType) {
        if (!IsValidNodeType(nodeType)) {
            FailOutOfRange(caller, nodeType);
        }
    }

    // 终止逻辑放在 .cpp 里:本头文件被大量工程包含,不在这里引入 muduo 日志。
    [[noreturn]] static void FailOutOfRange(const char* caller, uint32_t nodeType);

    NodeRegistries registries_;
    NodeGlobalEntity globalEntities_;
};

extern thread_local NodeContextManager tlsNodeContextManager;
