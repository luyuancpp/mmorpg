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
    // 不变量:actorRegistry 里任何实体的整数值都不是 0。
    // actor 句柄会以 to_integral 的形式下发给客户端(AOI、技能),协议里一律用 0 表示
    // 「没有目标 / 没有施法者」(uint64 默认值)。而全新 registry 第一次 create() 出来的
    // 正是 index 0、version 0 的实体,整数值就是 0 —— 本节点第一个登录的玩家会和
    // 「没有目标」撞在一起。这里在线程启动时 create 再 destroy 一次,让 0 号槽位的
    // version 变成 1:之后复用这个槽位的实体整数是 1 << 32,valid(entity{0}) 恒为 false。
    // registry.clear() 只会继续抬高 version,不会让这条不变量失效。
    EcsContext();

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
